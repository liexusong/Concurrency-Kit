#include <assert.h>
#include <ck_cc.h>
#include <ck_pr.h>
#ifdef SPINLOCK
#include <ck_spinlock.h>
#endif
#include <ck_stack.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef __linux__
#include <sched.h>
#include <sys/types.h>
#include <sys/syscall.h>
#endif

#ifndef CORES
#define CORES 8 
#endif

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#ifndef ITEMS
#define ITEMS (5765760 * 2)
#endif

#define TVTOD(tv) ((tv).tv_sec+((tv).tv_usec / (double)1000000))

struct affinity {
        uint32_t delta;
        uint32_t request;
};

struct entry {
	int value;
#ifdef SPINLOCK
	struct entry *next;
#else
	ck_stack_entry_t next;
#endif
};

#ifdef SPINLOCK
static struct entry *stack CK_CC_CACHELINE;
ck_spinlock_fas_t stack_spinlock = CK_SPINLOCK_FAS_INITIALIZER;
#define UNLOCK ck_spinlock_fas_unlock
#if defined(EB)
#define LOCK ck_spinlock_fas_lock_eb
#else
#define LOCK ck_spinlock_fas_lock
#endif
#else
static ck_stack_t stack CK_CC_CACHELINE;
CK_STACK_CONTAINER(struct entry, next, getvalue)
#endif

static struct affinity affinerator;
static unsigned long long nthr;
static volatile unsigned int barrier = 0;
static unsigned int critical;

#ifdef __linux__
#ifndef gettid
static pid_t
gettid(void)
{
        return syscall(__NR_gettid);
}
#endif

static int
aff_iterate(struct affinity *acb)
{
        cpu_set_t s;
        int c;

        c = ck_pr_faa_32(&acb->request, acb->delta);

        CPU_ZERO(&s);
        CPU_SET(c % CORES, &s);

        return sched_setaffinity(gettid(), sizeof(s), &s);
}
#else
static int
aff_iterate(struct affinity *acb)
{
	acb = NULL;
	return (0);
}
#endif

static void *
stack_thread(void *unused CK_CC_UNUSED)
{
#if (defined(MPMC) && defined(CK_F_STACK_POP_MPMC)) || (defined(UPMC) && defined(CK_F_STACK_POP_UPMC))
	ck_stack_entry_t *ref;
#endif
	struct entry *entry = NULL;
	unsigned long long i, n = ITEMS / nthr;
	unsigned int seed;
	int j, previous = INT_MAX;

	if (aff_iterate(&affinerator)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	while (barrier == 0);

	for (i = 0; i < n; i++) {
#ifdef MPMC
#ifdef CK_F_STACK_POP_MPMC
		ref = ck_stack_pop_mpmc(&stack);
		assert(ref);
		entry = getvalue(ref);
#endif
#elif defined(UPMC)
		ref = ck_stack_pop_upmc(&stack);
		assert(ref);
		entry = getvalue(ref);
#elif defined(SPINLOCK)
		LOCK(&stack_spinlock);
		entry = stack;
		stack = stack->next;
		UNLOCK(&stack_spinlock);
#else
#		error Undefined operation.
#endif

		if (critical) {
			j = rand_r(&seed) % critical;
			while (j--)
				__asm__ __volatile__("" ::: "memory");
		}

		assert (previous >= entry->value);
		previous = entry->value;
	}

	return (NULL);
}

static void 
stack_assert(void)
{

#ifdef SPINLOCK
	assert(stack == NULL);
#else
	assert(CK_STACK_ISEMPTY(&stack));
#endif
	return;
}

static void
push_stack(struct entry *bucket)
{
	unsigned long long i;

#ifdef SPINLOCK
	stack = NULL;
#else
	ck_stack_init(&stack);
#endif

	for (i = 0; i < ITEMS; i++) {
		bucket[i].value = i % INT_MAX;
#ifdef SPINLOCK
		bucket[i].next = stack;
		stack = bucket + i;
#else
		ck_stack_push_spnc(&stack, &bucket[i].next);
#endif
	}

#ifndef SPINLOCK
	ck_stack_entry_t *entry;
	i = 0;
	CK_STACK_FOREACH(&stack, entry) {
		i++;
	}
	assert(i == ITEMS);
#endif

	return;
}

int
main(int argc, char *argv[])
{
	struct entry *bucket;
	unsigned long long i, d, n;
	pthread_t *thread;
	struct timeval stv, etv;

#if defined(MPMC) && (!defined(CK_F_STACK_PUSH_MPMC) || !defined(CK_F_STACK_POP_MPMC))
	fprintf(stderr, "Unsupported.\n");
	return 0;
#endif

	if (argc != 4) {
		fprintf(stderr, "Usage: stack <threads> <delta> <critical>\n");
		exit(EXIT_FAILURE);
	}

	{
		char *e;

		nthr = strtol(argv[1], &e, 10);
		if (errno == ERANGE) {
			perror("ERROR: too many threads");
			exit(EXIT_FAILURE);
		} else if (*e != '\0') {
			fprintf(stderr, "ERROR: input format is incorrect\n");
			exit(EXIT_FAILURE);
		}

		d = strtol(argv[2], &e, 10);
		if (errno == ERANGE) {
			perror("ERROR: delta is too large");
			exit(EXIT_FAILURE);
		} else if (*e != '\0') {
			fprintf(stderr, "ERROR: input format is incorrect\n");
			exit(EXIT_FAILURE);
		}

		critical = strtoul(argv[3], &e, 10);
		if (errno == ERANGE) {
			perror("ERROR: critical section is too large");
			exit(EXIT_FAILURE);
		} else if (*e != '\0') {
			fprintf(stderr, "ERROR: input format is incorrect\n");
			exit(EXIT_FAILURE);
		}
	}

	srand(getpid());

	affinerator.request = 0;
	affinerator.delta = d;
	n = ITEMS / nthr;

	bucket = malloc(sizeof(struct entry) * ITEMS);
	assert(bucket != NULL);

	thread = malloc(sizeof(pthread_t) * nthr);
	assert(thread != NULL);

	push_stack(bucket);
	for (i = 0; i < nthr; i++)
		pthread_create(&thread[i], NULL, stack_thread, NULL);

	barrier = 1;

	for (i = 0; i < nthr; i++)
		pthread_join(thread[i], NULL);

	barrier = 0;

	push_stack(bucket);
	for (i = 0; i < nthr; i++)
		pthread_create(&thread[i], NULL, stack_thread, NULL);

	gettimeofday(&stv, NULL);
	barrier = 1;
	for (i = 0; i < nthr; i++)
		pthread_join(thread[i], NULL);
	gettimeofday(&etv, NULL);

	stack_assert();
	printf("%3llu %.6lf\n", nthr, TVTOD(etv) - TVTOD(stv));
	return 0;
}