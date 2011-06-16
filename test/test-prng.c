/*-
 * brian m. carlson <sandals@crustytoothpaste.ath.cx> wrote this source code.
 * This source code is in the public domain; you may do whatever you please with
 * it.  However, a credit in the documentation, although not required, would be
 * appreciated.
 */
/* This code implements a test driver for several different PRNGs.  This
 * implementation requires ANSI C and POSIX 1003.1-2001.
 */

#include "framework.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <drew/plugin.h>
#include <drew/prng.h>

int test_get_type(void)
{
	return DREW_TYPE_PRNG;
}

const char *test_get_default_algo(drew_loader_t *ldr, const char *name)
{
	return NULL;
}

int test_internal(drew_loader_t *ldr, const char *name, const void *tbl)
{
	const drew_prng_functbl_t *functbl = tbl;
	
	return print_test_results(functbl->test(NULL, ldr), NULL);
}

inline int test_speed_loop(drew_prng_t *ctx, uint8_t *buf,
		uint8_t *blk, int blksz, int chunk, int nchunks,
		const drew_loader_t *ldr)
{
	int i;

	ctx->functbl->init(ctx, 0, ldr, NULL);
	ctx->functbl->seed(ctx, blk, blksz, blksz);
	for (i = 0; !framework_sigflag && i < nchunks; i++)
		ctx->functbl->bytes(ctx, buf, chunk);
	ctx->functbl->fini(ctx, 0);

	return i;
}

// From http://graphics.stanford.edu/~seander/bithacks.html
static const uint8_t popcount[] = {
#define B2(n) n, n+1, n+1, n+2
#define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
#define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
	B6(0), B6(1), B6(1), B6(2)
};

// Tests return true if they pass and false otherwise.
static bool prng_test_monobit(const uint8_t *buf, size_t len)
{
	ssize_t nset = 0, total = len * 8, sn;
	double sobs;

	for (size_t i = 0; i < len; i++)
		nset += popcount[buf[i]];
	
	sn = nset - (total - nset);
	sobs = fabs(sn) / sqrt(total);
	return erfc(sobs / sqrt(2)) >= 0.01;
}

#define NBYTES (12 * 1024 * 1024)
int test_external(const drew_loader_t *ldr, const char *name, const void *tbl,
		const char *filename, struct test_external *tes)
{
	int ret = 0;
	drew_prng_t prng;
	uint8_t *p;
	
	if (!(p = malloc(NBYTES))) {
		ret = -ENOMEM;
		goto out;
	}

	prng.functbl = tbl;
	prng.functbl->init(&prng, 0, ldr, NULL);
	// This will trigger the autoseeding, if any.
	prng.functbl->bytes(&prng, p, NBYTES);
	// Seed the generator with part of whatever randomness may have been
	// produced.  We mark it as having no entropy, since we really can't be sure
	// what it contains.
	prng.functbl->seed(&prng, p, NBYTES >> 4, 0);
	// Use this for the tests.
	prng.functbl->bytes(&prng, p, NBYTES);
	prng.functbl->fini(&prng, 0);

	ret |= !prng_test_monobit(p, NBYTES);
out:
	free(p);
	return print_test_results(ret, NULL);
}

int test_external_parse(const drew_loader_t *ldr, const char *filename,
		struct test_external *tes)
{
	return 0;
}

#define STUBS_API 1
#include "stubs.c"

int test_speed(drew_loader_t *ldr, const char *name, const char *algo,
		const void *tbl, int chunk, int nchunks)
{
	int i, blksz = 0;
	uint8_t *buf, *blk;
	struct timespec cstart, cend;
	drew_prng_t ctx;
	void *fwdata;

	ctx.functbl = tbl;

	blksz = ctx.functbl->info(DREW_PRNG_BLKSIZE, NULL);
	buf = calloc(chunk, 1);
	if (!buf)
		return ENOMEM;

	blk = malloc(blksz);
	if (!blk)
		return ENOMEM;

	fwdata = framework_setup();

	clock_gettime(USED_CLOCK, &cstart);
	i = test_speed_loop(&ctx, buf, blk, blksz, chunk, nchunks, ldr);
	clock_gettime(USED_CLOCK, &cend);

	framework_teardown(fwdata);

	free(buf);
	free(blk);

	print_speed_info(chunk, i, &cstart, &cend);
	
	return 0;
}
