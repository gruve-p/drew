/*-
 * Copyright © 2010–2011 brian m. carlson
 *
 * This file is part of the Drew Cryptography Suite.
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of your choice of version 2 of the GNU General Public License as
 * published by the Free Software Foundation or version 2.0 of the Apache
 * License as published by the Apache Software Foundation.
 *
 * This file is distributed in the hope that it will be useful, but without
 * any warranty; without even the implied warranty of merchantability or fitness
 * for a particular purpose.
 *
 * Note that people who make modified versions of this file are not obligated to
 * dual-license their modified versions; it is their choice whether to do so.
 * If a modified version is not distributed under both licenses, the copyright
 * and permission notices should be updated accordingly.
 */
/* This file is designed to produce the Twofish sboxes q0 and q1.  It is not at
 * all optimized, since this is expected to really only be run once.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* These sboxes are specified in the Twofish paper. */
const uint8_t t0[4][16] = {
	{
		0x8, 0x1, 0x7, 0xd, 0x6, 0xf, 0x3, 0x2,
		0x0, 0xb, 0x5, 0x9, 0xe, 0xc, 0xa, 0x4
	},
	{
		0xe, 0xc, 0xb, 0x8, 0x1, 0x2, 0x3, 0x5,
		0xf, 0x4, 0xa, 0x6, 0x7, 0x0, 0x9, 0xd
	},
	{
		0xb, 0xa, 0x5, 0xe, 0x6, 0xd, 0x9, 0x0,
		0xc, 0x8, 0xf, 0x3, 0x2, 0x4, 0x7, 0x1
	},
	{
		0xd, 0x7, 0xf, 0x4, 0x1, 0x2, 0x6, 0xe,
		0x9, 0xb, 0x3, 0x0, 0x8, 0x5, 0xc, 0xa
	}
};

const uint8_t t1[4][16] = {
	{
		0x2, 0x8, 0xb, 0xd, 0xf, 0x7, 0x6, 0xe,
		0x3, 0x1, 0x9, 0x4, 0x0, 0xa, 0xc, 0x5
	},
	{
		0x1, 0xe, 0x2, 0xb, 0x4, 0xc, 0x3, 0x7,
		0x6, 0xd, 0xa, 0x5, 0xf, 0x9, 0x0, 0x8
	},
	{
		0x4, 0xc, 0x7, 0x5, 0x1, 0x6, 0x9, 0xa,
		0x0, 0xe, 0xd, 0x8, 0x2, 0xb, 0x3, 0xf
	},
	{
		0xb, 0x9, 0x5, 0x1, 0xc, 0x3, 0xd, 0xe,
		0x6, 0x4, 0x7, 0xf, 0x2, 0x0, 0x8, 0xa
	}
};

const uint8_t mds[4][4] = {
	{0x01, 0xef, 0x5b, 0x5b},
	{0x5b, 0xef, 0xef, 0x01},
	{0xef, 0x5b, 0x01, 0xef},
	{0xef, 0x01, 0xef, 0x5b}
};

/* Algorithm from http://www.cs.utsa.edu/~wagner/laws/FFM.html */
uint8_t ffmul(uint8_t a, uint8_t b, uint8_t poly)
{
	uint8_t r = 0, t;
	while (a) {
		if (a & 1)
			r ^= b;
		t = b & 0x80;
		b <<= 1;
		if (t)
			b ^= poly;
		a >>= 1;
	}
	return r;
}

uint8_t ror1(uint8_t x)
{
	return ((x >> 1) | (x << 3)) & 0xf;
}

uint8_t algo(const uint8_t (*t)[16], uint8_t x)
{
	uint8_t a0, b0, a1, b1, a2, b2, a3, b3, a4, b4;

	a0 = x >> 4;
	b0 = x & 0xf;
	a1 = a0 ^ b0;
	b1 = a0 ^ ror1(b0) ^ (a0 << 3);
	a2 = t[0][a1];
	b2 = t[1][b1 & 0xf];
	a3 = a2 ^ b2;
	b3 = a2 ^ ror1(b2) ^ (a2 << 3);
	a4 = t[2][a3];
	b4 = t[3][b3 & 0xf];
	return (b4 << 4) | a4;
}

void print_table(const char *name, const uint8_t (*t)[16])
{
	printf("const uint8_t drew::Twofish::%s[] = {", name);
	for (int i = 0; i < 256; i++) {
		if (!(i & 7))
			printf("\n\t");
		printf("0x%02x, ", algo(t, i));
	}
	printf("\n};\n");
}

void test(const uint8_t (*t)[16])
{
	for (int i = 0; i < 4; i++) {
		uint8_t tbl[16];
		memset(tbl, 0, sizeof(tbl));
		for (int j = 0; j < 16; j++)
			tbl[j]++;
		for (int j = 0; j < 16; j++)
			if (tbl[j] != 1)
				abort();
	}
}

int main(void)
{
	test(t0);
	test(t1);
	print_table("q0", t0);
	print_table("q1", t1);
}
