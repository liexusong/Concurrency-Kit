#include <ck_bytelock.h>
#include <ck_spinlock.h>
#include <inttypes.h>
#include <stdio.h>

#ifndef STEPS 
#define STEPS 100000ULL
#endif

/*
 * This is a naive reader/writer spinlock.
 */
struct rwlock {
	unsigned int readers;
	ck_spinlock_fas_t writer;
};
typedef struct rwlock rwlock_t;

static CK_CC_INLINE void
rwlock_init(rwlock_t *rw)
{

	ck_pr_store_uint(&rw->readers, 0);
	ck_spinlock_fas_init(&rw->writer);
	return;
}

static CK_CC_INLINE void
rwlock_write_lock(rwlock_t *rw)
{

	ck_spinlock_fas_lock(&rw->writer);
	while (ck_pr_load_uint(&rw->readers) != 0)
		ck_pr_stall();

	return;
}

static CK_CC_INLINE void
rwlock_write_unlock(rwlock_t *rw)
{

	ck_spinlock_fas_unlock(&rw->writer);
	return;
}

static CK_CC_INLINE void
rwlock_read_lock(rwlock_t *rw)
{

	for (;;) {
		while (ck_pr_load_uint(&rw->writer.value) != 0)
			ck_pr_stall();

		ck_pr_inc_uint(&rw->readers);
		if (ck_pr_load_uint(&rw->writer.value) == 0)
			break;
		ck_pr_dec_uint(&rw->readers);
	}

	return;
}

static CK_CC_INLINE void
rwlock_read_unlock(rwlock_t *rw)
{

	ck_pr_dec_uint(&rw->readers);
	return;
}


static inline uint64_t
rdtsc(void)
{
#if defined(__x86_64__)
        uint32_t eax = 0, edx;

	__asm__ __volatile__("cpuid;"
			     "rdtsc;"
				: "+a" (eax), "=d" (edx)
				:
				: "%rcx", "%rbx", "memory");

	__asm__ __volatile__("xorl %%eax, %%eax;"
			     "cpuid;"
				:
				:
				: "%rax", "%rbx", "%rcx", "%rdx", "memory");

	return (((uint64_t)edx << 32) | eax);
#endif

	return 0;
}

int
main(void)
{
	uint64_t s_b, e_b;
	uint64_t i;
	ck_bytelock_t bytelock = CK_BYTELOCK_INITIALIZER;
	rwlock_t naive;

	ck_bytelock_write_lock(&bytelock, 1);
	ck_bytelock_write_unlock(&bytelock);

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		ck_bytelock_write_lock(&bytelock, 1);
		ck_bytelock_write_unlock(&bytelock);
	}
	e_b = rdtsc();
	printf("WRITE: bytelock %15" PRIu64 "\n", e_b - s_b);

	rwlock_init(&naive);
	rwlock_write_lock(&naive);
	rwlock_write_unlock(&naive);

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		rwlock_write_lock(&naive);
		rwlock_write_unlock(&naive);
	}
	e_b = rdtsc();
	printf("WRITE: naive    %15" PRIu64 "\n", e_b - s_b);

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		ck_bytelock_read_lock(&bytelock, 1);
		ck_bytelock_read_unlock(&bytelock, 1);
	}
	e_b = rdtsc();
	printf("READ:  bytelock %15" PRIu64 "\n", e_b - s_b);

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		rwlock_write_lock(&naive);
		rwlock_write_unlock(&naive);
	}
	e_b = rdtsc();
	printf("READ:  naive    %15" PRIu64 "\n", e_b - s_b);

	return (0);
}