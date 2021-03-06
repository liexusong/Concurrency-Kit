/*
 * Copyright 2009-2011 Samy Al Bahra.
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

#ifndef _CK_BACKOFF_H
#define _CK_BACKOFF_H

#include <ck_cc.h>

#ifndef CK_BACKOFF_CEILING
#define CK_BACKOFF_CEILING ((1 << 21) - 1)
#endif

#define CK_BACKOFF_INITIALIZER ((1 << 9) - 1) 

typedef volatile unsigned int ck_backoff_t;

/*
 * This is an exponential back-off implementation.
 */
CK_CC_INLINE static void
ck_backoff_eb(volatile unsigned int *c)
{
	volatile unsigned int i;
	unsigned int ceiling;
	
	ceiling = *c;

	for (i = 0; i < ceiling; i++);

	ceiling *= ceiling;
	ceiling &= CK_BACKOFF_CEILING;
	ceiling += CK_BACKOFF_INITIALIZER;

	*c = ceiling;
	return;
}

/*
 * This is a geometric back-off implementation.
 */
CK_CC_INLINE static void
ck_backoff_gb(volatile unsigned int *c)
{
	volatile unsigned int i;
	unsigned int ceiling;

	ceiling = *c;

	for (i = 0; i < ceiling; i++);

	ceiling <<= 1;
	ceiling &= CK_BACKOFF_CEILING;

	*c = ceiling;
	return;
}

#endif /* _CK_BACKOFF_H */
