/*
 * Copyright 2011 Samy Al Bahra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <ck_fifo.h>

#include "../../common.h"

#ifdef CK_F_FIFO_MPMC
#ifndef ITERATIONS
#define ITERATIONS 128 
#endif

struct context {
	unsigned int tid;
	unsigned int previous;
	unsigned int next;
};

struct entry {
	int tid;
	int value;
};

static int nthr;

#ifdef CK_F_FIFO_MPMC
static ck_fifo_mpmc_t fifo;
#endif

static struct affinity a;
static int size;
static unsigned int barrier;

static void *
test(void *c)
{
#ifdef CK_F_FIFO_MPMC
	struct context *context = c;
	struct entry *entry;
	ck_fifo_mpmc_entry_t *fifo_entry, *garbage;
	int i, j;

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < (unsigned int)nthr);

	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < size; j++) {
			fifo_entry = malloc(sizeof(ck_fifo_mpmc_entry_t));
			entry = malloc(sizeof(struct entry));
			entry->tid = context->tid;
			ck_fifo_mpmc_enqueue(&fifo, fifo_entry, entry);
			if (ck_fifo_mpmc_dequeue(&fifo, &entry, &garbage) == false) {
				fprintf(stderr, "ERROR [%u] Queue should never be empty.\n", context->tid);
				exit(EXIT_FAILURE);
			}

			if (entry->tid < 0 || entry->tid >= nthr) {
				fprintf(stderr, "ERROR [%u] Incorrect value in entry.\n", entry->tid);
				exit(EXIT_FAILURE);
			}
		}
	}
#endif

	return (NULL);
}

int
main(int argc, char *argv[])
{
	int i, r;
	struct context *context;
	pthread_t *thread;

	if (argc != 4) {
		fprintf(stderr, "Usage: validate <threads> <affinity delta> <size>\n");
		exit(EXIT_FAILURE);
	}

	a.request = 0;
	a.delta = atoi(argv[2]);

	nthr = atoi(argv[1]);
	assert(nthr >= 1);

	size = atoi(argv[3]);
	assert(size > 0);

	context = malloc(sizeof(*context) * nthr);
	assert(context);

	thread = malloc(sizeof(pthread_t) * nthr);
	assert(thread);

	ck_fifo_mpmc_init(&fifo, malloc(sizeof(ck_fifo_mpmc_entry_t)));
	for (i = 0; i < nthr; i++) {
		context[i].tid = i;
		r = pthread_create(thread + i, NULL, test, context + i);
		assert(r == 0);
	}

	for (i = 0; i < nthr; i++)
		pthread_join(thread[i], NULL);

	return (0);
}
#else
int
main(void)
{
	fprintf(stderr, "Unsupported.\n");
	return 0;
}
#endif

