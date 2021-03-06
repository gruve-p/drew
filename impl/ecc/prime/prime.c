/*-
 * Copyright © 2000-2008 The Legion Of The Bouncy Castle
 * (http://www.bouncycastle.org)
 * Copyright © 2011 brian m. carlson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "internal.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <drew/drew.h>
#include <drew/bignum.h>
#include <drew/ecc.h>
#include <drew/mem.h>
#include <drew/param.h>
#include <drew/plugin.h>

#include "util.h"

struct point {
	bool inf;
	drew_bignum_t x;
	drew_bignum_t y;
	struct curve *curve;
};

struct curve {
	const char *name;
	drew_bignum_t p;
	drew_bignum_t a;
	drew_bignum_t b;
	struct point g;
	drew_bignum_t n;
	drew_bignum_t h;
};

static int ecp_info(int op, void *p);
static int ecp_info2(const drew_ecc_t *ctx, int op, drew_param_t *out,
		const drew_param_t *in);
static int ecp_init(drew_ecc_t *ctx, int flags, DrewLoader *ldr,
		const drew_param_t *param);
static int ecp_clone(drew_ecc_t *new, const drew_ecc_t *old, int flags);
static int ecp_fini(drew_ecc_t *ctx, int flags);
static int ecp_setcurvename(drew_ecc_t *ctx, const char *name);
static int ecp_curvename(drew_ecc_t *ctx, const char **namep);
static int ecp_setval(drew_ecc_t *ctx, const char *name, const uint8_t *data,
		size_t len, int coord);
static int ecp_val(const drew_ecc_t *ctx, const char *name, uint8_t *data,
		size_t len, int coord);
static int ecp_valsize(const drew_ecc_t *, const char *, int);
static int ecp_setvalbignum(drew_ecc_t *ctx, const char *name,
		const drew_bignum_t *bn, int coord);
static int ecp_valbignum(const drew_ecc_t *ctx , const char *name,
		drew_bignum_t *bn, int coord);
static int ecp_setvalpoint(drew_ecc_t *ctx, const char *name,
		const drew_ecc_point_t *pt);
static int ecp_valpoint(const drew_ecc_t *ctx, const char *name,
		drew_ecc_point_t *pt);
static int ecp_point(const drew_ecc_t *ctx, drew_ecc_point_t *pt);
static int ecp_test(void *, DrewLoader *);

static int ecpt_info(int op, void *p);
static int ecpt_info2(const drew_ecc_point_t *ctx, int op, drew_param_t *out,
		const drew_param_t *in);
static int ecpt_init(drew_ecc_point_t *ctx, int flags,
		DrewLoader *ldr, const drew_param_t *param);
static int ecpt_clone(drew_ecc_point_t *new, const drew_ecc_point_t *old,
		int flags);
static int ecpt_fini(drew_ecc_point_t *ctx, int flags);
static int ecpt_setinf(drew_ecc_point_t *ctx, bool val);
static int ecpt_isinf(const drew_ecc_point_t *ctx);
static int ecpt_compare(const drew_ecc_point_t *a, const drew_ecc_point_t *b);
static int ecpt_setcoordbytes(drew_ecc_point_t *ctx, const uint8_t *data,
		size_t len, int coord);
static int ecpt_coordbytes(const drew_ecc_point_t *ctx, uint8_t *data,
		size_t len, int coord);
static int ecpt_ncoordbytes(const drew_ecc_point_t *ctx, int coord);
static int ecpt_setcoordbignum(drew_ecc_point_t *ctx, const drew_bignum_t *data,
		int coord);
static int ecpt_coordbignum(const drew_ecc_point_t *ctx,
		drew_bignum_t *bn, int coord);
static int ecpt_inv(drew_ecc_point_t *r, const drew_ecc_point_t *a);
static int ecpt_dbl(drew_ecc_point_t *ptr, const drew_ecc_point_t *pta);
static int ecpt_add(drew_ecc_point_t *ptr, const drew_ecc_point_t *pta,
		const drew_ecc_point_t *ptb);
static int ecpt_mul2(drew_ecc_point_t *r, const drew_ecc_point_t *ptp,
		const drew_bignum_t *a, const drew_ecc_point_t *ptq,
		const drew_bignum_t *b);
static int ecpt_mul(drew_ecc_point_t *ptr, const drew_ecc_point_t *pta,
		const drew_bignum_t *b);
static int ecpt_test(void *p, DrewLoader *ldr);

static drew_ecc_functbl_t ecp_functbl = {
	ecp_info, ecp_info2, ecp_init, ecp_clone, ecp_fini, ecp_setcurvename,
	ecp_curvename, ecp_setval, ecp_val, ecp_valsize, ecp_setvalbignum,
	ecp_valbignum, ecp_setvalpoint, ecp_valpoint, ecp_point, ecp_test
};

static drew_ecc_point_functbl_t ecpt_functbl = {
	ecpt_info, ecpt_info2, ecpt_init, ecpt_clone, ecpt_fini, ecpt_setinf,
	ecpt_isinf, ecpt_compare, ecpt_setcoordbytes, ecpt_coordbytes,
	ecpt_ncoordbytes, ecpt_setcoordbignum, ecpt_coordbignum, ecpt_inv, ecpt_add,
	ecpt_mul, ecpt_mul2, ecpt_dbl, ecpt_test
};



static int ecp_info(int op, void *p)
{
	switch (op) {
		case DREW_ECC_VERSION:
			return CURRENT_ABI;
		case DREW_ECC_INTSIZE:
			return sizeof(struct curve);
		default:
			return -DREW_ERR_INVALID;
	}
}

static int ecp_info2(const drew_ecc_t *ctx, int op, drew_param_t *out,
		const drew_param_t *in)
{
	return ecp_info(op, NULL);
}

static int ecp_init(drew_ecc_t *ctx, int flags, DrewLoader *ldr,
		const drew_param_t *param)
{
	drew_bignum_t *bn = 0;
	struct curve *c;

	for (const drew_param_t *p = param; p; p = p->next)
		if (!strcmp(p->name, "bignum"))
			bn = p->param.value;

	if (!bn)
		return -DREW_ERR_MORE_INFO;

	if (!(flags & DREW_ECC_FIXED))
		if (!(ctx->ctx = drew_mem_malloc(sizeof(struct curve))))
			return -ENOMEM;

	c = ctx->ctx;
	ctx->functbl = &ecp_functbl;
	c->name = NULL;
	bn->functbl->clone(&c->p, bn, 0);
	bn->functbl->clone(&c->a, bn, 0);
	bn->functbl->clone(&c->b, bn, 0);
	bn->functbl->clone(&c->n, bn, 0);
	bn->functbl->clone(&c->h, bn, 0);
	bn->functbl->clone(&c->g.x, bn, 0);
	bn->functbl->clone(&c->g.y, bn, 0);
	c->g.curve = c;

	return 0;
}

static int ecp_clone(drew_ecc_t *new, const drew_ecc_t *old, int flags)
{
	struct curve *cn, *co = old->ctx;
	int flg = flags & DREW_ECC_COPY ? DREW_BIGNUM_COPY : 0;

	if (!(flags & (DREW_ECC_FIXED | DREW_ECC_COPY)))
		if (!(new->ctx = drew_mem_malloc(sizeof(struct curve))))
			return -ENOMEM;

	new->functbl = &ecp_functbl;
	cn = new->ctx;
	cn->name = co->name;
	memcpy(cn, co, sizeof(*cn));
	cn->p.functbl->clone(&cn->p, &co->p, flg);
	cn->a.functbl->clone(&cn->a, &co->a, flg);
	cn->b.functbl->clone(&cn->b, &co->b, flg);
	cn->n.functbl->clone(&cn->n, &co->n, flg);
	cn->h.functbl->clone(&cn->h, &co->h, flg);
	cn->g.x.functbl->clone(&cn->g.x, &co->g.x, flg);
	cn->g.y.functbl->clone(&cn->g.y, &co->g.y, flg);
	cn->g.curve = cn;
	return 0;
}

static int ecp_fini(drew_ecc_t *ctx, int flags)
{
	struct curve *c = ctx->ctx;

	c->p.functbl->fini(&c->p, 0);
	c->a.functbl->fini(&c->a, 0);
	c->b.functbl->fini(&c->b, 0);
	c->n.functbl->fini(&c->n, 0);
	c->h.functbl->fini(&c->h, 0);
	c->g.x.functbl->fini(&c->g.x, 0);
	c->g.y.functbl->fini(&c->g.y, 0);

	if (!(flags & DREW_ECC_FIXED)) {
		drew_mem_free(c);
		ctx->ctx = NULL;
	}

	return 0;
}

static int load_bignum(drew_bignum_t *b, const uint8_t *p, size_t len)
{
	return b->functbl->setbytes(b, p, len);
}

static int store_point(const struct point *pt, uint8_t *p, size_t len)
{
	if (!len)
		return -DREW_ERR_INVALID;
	if (pt->inf) {
		*p = 0;
		return 1;
	}
	size_t nbytes = pt->curve->p.functbl->nbytes(&pt->curve->p);
	if (len < (nbytes * 2) + 1)
		return -DREW_ERR_MORE_INFO;
	memset(p, 0, len);
	size_t xbytes = pt->x.functbl->nbytes(&pt->x);
	size_t ybytes = pt->y.functbl->nbytes(&pt->y);
	*p = 4;
	pt->x.functbl->bytes(&pt->x, p+1+(nbytes-xbytes), xbytes);
	pt->y.functbl->bytes(&pt->y, p+1+nbytes+(nbytes-ybytes), ybytes);
	return 0;
}

static int load_point(struct point *pt, const uint8_t *p, size_t len)
{
	const size_t piecelen = len / 2;

	if (!len)
		return -DREW_ERR_INVALID;
	if (*p == 0) {
		pt->inf = true;
		return 0;
	}
	if (*p != 4)
		return -DREW_ERR_NOT_IMPL;

	pt->inf = false;
	RETFAIL(pt->x.functbl->setbytes(&pt->x, p+1, piecelen));
	RETFAIL(pt->y.functbl->setbytes(&pt->y, p+piecelen+1, piecelen));
	return 0;
}

struct curve_vals {
	const char *name;
	const char *p;
	const char *a;
	const char *b;
	const char *g;
	const char *n;
	const char *h;
};

static const struct curve_vals curves[] = {
	{
		"secp192k1",
		"fffffffffffffffffffffffffffffffffffffffeffffee37",
		"00",
		"03",
		"04db4ff10ec057e9ae26b07d0280b7f4341da5d1b1eae06c7d9b2f2f6d9c5628a7844"
			"163d015be86344082aa88d95e2f9d",
		"fffffffffffffffffffffffe26f2fc170f69466a74defd8d",
		"01"
	},
	{
		"secp192r1",
		"fffffffffffffffffffffffffffffffeffffffffffffffff",
		"fffffffffffffffffffffffffffffffefffffffffffffffc",
		"64210519e59c80e70fa7e9ab72243049feb8deecc146b9b1",
		"04188da80eb03090f67cbf20eb43a18800f4ff0afd82ff101207192b95ffc8da78631"
			"011ed6b24cdd573f977a11e794811",
		"ffffffffffffffffffffffff99def836146bc9b1b4d22831",
		"01"
	},
	{
		"secp224k1",
		"fffffffffffffffffffffffffffffffffffffffffffffffeffffe56d",
		"00",
		"05",
		"04a1455b334df099df30fc28a169a467e9e47075a90f7e650eb6b7a45c7e089fed7fb"
			"a344282cafbd6f7e319f7c0b0bd59e2ca4bdb556d61a5",
		"010000000000000000000000000001dce8d2ec6184caf0a971769fb1f7",
		"01"
	},
	{
		"secp224r1",
		"ffffffffffffffffffffffffffffffff000000000000000000000001",
		"fffffffffffffffffffffffffffffffefffffffffffffffffffffffe",
		"b4050a850c04b3abf54132565044b0b7d7bfd8ba270b39432355ffb4",
		"04b70e0cbd6bb4bf7f321390b94a03c1d356c21122343280d6115c1d21bd376388b5f"
			"723fb4c22dfe6cd4375a05a07476444d5819985007e34",
		"ffffffffffffffffffffffffffff16a2e0b8f03e13dd29455c5c2a3d",
		"01"
	},
	{
		"secp256k1",
		"fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
		"00",
		"07",
		"0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483"
			"ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8",
		"fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141",
		"01"
	},
	{
		"secp256r1",
		"ffffffff00000001000000000000000000000000ffffffffffffffffffffffff",
		"ffffffff00000001000000000000000000000000fffffffffffffffffffffffc",
		"5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b",
		"046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c2964fe"
			"342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5",
		"ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551",
		"01"
	},
	{
		"secp384r1",
		"fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffefffff"
			"fff0000000000000000ffffffff",
		"fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffefffff"
			"fff0000000000000000fffffffc",
		"b3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875ac6563"
			"98d8a2ed19d2a85c8edd3ec2aef",
		"04aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a38550"
			"2f25dbf55296c3a545e3872760ab73617de4a96262c6f5d9e98bf9292dc29f8f4"
			"1dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f",
		"ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf581a0"
			"db248b0a77aecec196accc52973",
		"01"
	},
	{
		"secp521r1",
		"01fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
			"fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
		"01fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
			"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc",
		"0051953eb9618e1c9a1f929a21a0b68540eea2da725b99b315f3b8b489918ef109e15"
			"6193951ec7e937b1652c0bd3bb1bf073573df883d2c34f1ef451fd46b503f00",
		"0400c6858e06b70404e9cd9e3ecb662395b4429c648139053fb521f828af606b4d3db"
			"aa14b5e77efe75928fe1dc127a2ffa8de3348b3c1856a429bf97e7e31c2e5bd66"
			"011839296a789a3bc0045c8a5fb42c7d1bd998f54449579b446817afbd17273e6"
			"62c97ee72995ef42640c550b9013fad0761353c7086a272c24088be94769fd166"
			"50",
		"01fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa5"
			"1868783bf2f966b7fcc0148f709a5d03bb5c9b8899c47aebb6fb71e91386409",
		"01"
	}
};

static int strtobytes(uint8_t *buf, size_t len, const char *s)
{
	const size_t slen = strlen(s);
	if (slen & 1)
		return -DREW_ERR_INVALID;
	if (len < slen/2)
		return -DREW_ERR_BUG;

	unsigned x;
	for (size_t i = 0; i < (slen / 2); i++) {
		sscanf(s+(i*2), "%02x", &x);
		buf[i] = x;
	}
	return slen / 2;
}

#define MAX_CURVE_BITS 521
#define MAX_CURVE_BYTES ((MAX_CURVE_BITS + 7) / 8)
static int load_curve_bignum(drew_bignum_t *b, const char *p)
{
	uint8_t buf[(MAX_CURVE_BYTES + 1) + 1];
	int len = 0;
	if ((len = strtobytes(buf, sizeof(buf), p)) < 0)
		return len;
	RETFAIL(load_bignum(b, buf, len));
	return 0;
}

static int load_curve(struct curve *c, const char *name)
{
	int len = 0;
	uint8_t buf[((MAX_CURVE_BYTES + 1) * 2) + 1];

	for (size_t i = 0; i < DIM(curves); i++) {
		if (!strcmp(name, curves[i].name)) {
			c->name = curves[i].name;
			RETFAIL(load_curve_bignum(&c->p, curves[i].p));
			RETFAIL(load_curve_bignum(&c->a, curves[i].a));
			RETFAIL(load_curve_bignum(&c->b, curves[i].b));
			RETFAIL(load_curve_bignum(&c->n, curves[i].n));
			RETFAIL(load_curve_bignum(&c->h, curves[i].h));
			if ((len = strtobytes(buf, sizeof(buf), curves[i].g)) < 0)
				return len;
			RETFAIL(load_point(&c->g, buf, len));
			return 0;
		}
	}
	return -DREW_ERR_NOT_IMPL;
}

static int ecp_setcurvename(drew_ecc_t *ctx, const char *name)
{
	return load_curve(ctx->ctx, name);
}

static int ecp_curvename(drew_ecc_t *ctx, const char **namep)
{
	struct curve *c = ctx->ctx;

	*namep = c->name;

	return 0;
}

static int ecp_setval(drew_ecc_t *ctx, const char *name, const uint8_t *data,
		size_t len, int coord)
{
	struct curve *c = ctx->ctx;

	if (!strcmp(name, "p"))
		return load_bignum(&c->p, data, len);
	else if (!strcmp(name, "a"))
		return load_bignum(&c->a, data, len);
	else if (!strcmp(name, "b"))
		return load_bignum(&c->b, data, len);
	else if (!strcmp(name, "n"))
		return load_bignum(&c->n, data, len);
	else if (!strcmp(name, "h"))
		return load_bignum(&c->h, data, len);
	else if (!strcmp(name, "g")) {
		c->g.inf = false;
		if (coord == DREW_ECC_POINT_SEC ||
				coord == DREW_ECC_POINT_SEC_COMPRESSED)
			return load_point(&c->g, data, len);
		else if (coord == DREW_ECC_POINT_X)
			return load_bignum(&c->g.x, data, len);
		else if (coord == DREW_ECC_POINT_Y)
			return load_bignum(&c->g.y, data, len);
	}

	return -DREW_ERR_INVALID;
}

static int ecp_val(const drew_ecc_t *ctx, const char *name, uint8_t *data,
		size_t len, int coord)
{
	struct curve *c = ctx->ctx;
	const drew_bignum_functbl_t *ft = c->p.functbl;

	if (!strcmp(name, "p"))
		return ft->bytes(&c->p, data, len);
	else if (!strcmp(name, "a"))
		return ft->bytes(&c->a, data, len);
	else if (!strcmp(name, "b"))
		return ft->bytes(&c->b, data, len);
	else if (!strcmp(name, "n"))
		return ft->bytes(&c->n, data, len);
	else if (!strcmp(name, "h"))
		return ft->bytes(&c->h, data, len);
	else if (!strcmp(name, "g")) {
		c->g.inf = false;
		if (coord == DREW_ECC_POINT_SEC)
			return store_point(&c->g, data, len);
		else if (coord == DREW_ECC_POINT_SEC_COMPRESSED)
			return -DREW_ERR_NOT_IMPL;
		else if (coord == DREW_ECC_POINT_X)
			return ft->bytes(&c->g.x, data, len);
		else if (coord == DREW_ECC_POINT_Y)
			return ft->bytes(&c->g.y, data, len);
	}

	return -DREW_ERR_INVALID;
}

static int ecp_valsize(const drew_ecc_t *ctx, const char *name, int coord)
{
	struct curve *c = ctx->ctx;
	const drew_bignum_functbl_t *ft = c->p.functbl;

	if (!strcmp(name, "p"))
		return ft->nbytes(&c->p);
	else if (!strcmp(name, "a"))
		return ft->nbytes(&c->a);
	else if (!strcmp(name, "b"))
		return ft->nbytes(&c->b);
	else if (!strcmp(name, "n"))
		return ft->nbytes(&c->n);
	else if (!strcmp(name, "h"))
		return ft->nbytes(&c->h);
	else if (!strcmp(name, "g")) {
		if (coord == DREW_ECC_POINT_SEC)
			return c->g.inf ? 1 : (ft->nbytes(&c->p) * 2) + 1;
		else if (coord == DREW_ECC_POINT_SEC_COMPRESSED)
			return -DREW_ERR_NOT_IMPL;
		else if (coord == DREW_ECC_POINT_X)
			return ft->nbytes(&c->g.x);
		else if (coord == DREW_ECC_POINT_Y)
			return ft->nbytes(&c->g.y);
	}

	return -DREW_ERR_INVALID;
}

static int copy(drew_bignum_t *r, const drew_bignum_t *a)
{
	r->functbl->fini(r, 0);
	return r->functbl->clone(r, a, 0);
}

static int ecp_setvalbignum(drew_ecc_t *ctx, const char *name,
		const drew_bignum_t *bn, int coord)
{
	struct curve *c = ctx->ctx;

	if (!strcmp(name, "p"))
		return copy(&c->p, bn);
	else if (!strcmp(name, "a"))
		return copy(&c->a, bn);
	else if (!strcmp(name, "b"))
		return copy(&c->b, bn);
	else if (!strcmp(name, "n"))
		return copy(&c->n, bn);
	else if (!strcmp(name, "h"))
		return copy(&c->h, bn);
	else if (!strcmp(name, "g")) {
		if (coord == DREW_ECC_POINT_X)
			return copy(&c->g.x, bn);
		else if (coord == DREW_ECC_POINT_Y)
			return copy(&c->g.y, bn);
	}

	return -DREW_ERR_INVALID;
}

static int ecp_valbignum(const drew_ecc_t *ctx , const char *name,
		drew_bignum_t *bn, int coord)
{
	struct curve *c = ctx->ctx;

	if (!strcmp(name, "p"))
		return copy(bn, &c->p);
	else if (!strcmp(name, "a"))
		return copy(bn, &c->a);
	else if (!strcmp(name, "b"))
		return copy(bn, &c->b);
	else if (!strcmp(name, "n"))
		return copy(bn, &c->n);
	else if (!strcmp(name, "h"))
		return copy(bn, &c->h);
	else if (!strcmp(name, "g")) {
		if (coord == DREW_ECC_POINT_X)
			return copy(bn, &c->g.x);
		else if (coord == DREW_ECC_POINT_Y)
			return copy(bn, &c->g.y);
	}

	return -DREW_ERR_INVALID;
}

static int ecp_setvalpoint(drew_ecc_t *ctx, const char *name,
		const drew_ecc_point_t *pt)
{
	struct curve *c = ctx->ctx;
	drew_ecc_point_t g;

	if (strcmp(name, "g"))
		return -DREW_ERR_INVALID;
	g.ctx = &c->g;
	g.functbl = &ecpt_functbl;
	g.functbl->fini(&g, DREW_ECC_FIXED);
	g.functbl->clone(&g, pt, 0);
	return 0;
}

static int ecp_valpoint(const drew_ecc_t *ctx, const char *name,
		drew_ecc_point_t *pt)
{
	struct curve *c = ctx->ctx;
	drew_ecc_point_t g;

	if (strcmp(name, "g"))
		return -DREW_ERR_INVALID;

	g.ctx = &c->g;
	g.functbl = &ecpt_functbl;
	pt->functbl->fini(pt, 0);
	pt->functbl->clone(pt, &g, 0);
	return 0;
}

static int ecpt_init(drew_ecc_point_t *ctx, int flags,
		DrewLoader *ldr, const drew_param_t *param);

static int ecp_point(const drew_ecc_t *ctx, drew_ecc_point_t *pt)
{
	drew_param_t param;
	param.name = "curve";
	param.param.value = (void *)ctx;
	param.next = NULL;
	return ecpt_init(pt, 0, NULL, &param);
}

struct ecp_testcase {
	int k;
	const char *x;
	const char *y;
};

static struct ecp_testcase p521_testcases[] = {
	{
		1,
		"00c6858e06b70404e9cd9e3ecb662395b4429c648139053fb521f828af606b4d3dbaa"
			"14b5e77efe75928fe1dc127a2ffa8de3348b3c1856a429bf97e7e31c2e5bd66",
		"011839296a789a3bc0045c8a5fb42c7d1bd998f54449579b446817afbd17273e662c9"
			"7ee72995ef42640c550b9013fad0761353c7086a272c24088be94769fd16650"
	},
	{
		2,
		"00433c219024277e7e682fcb288148c282747403279b1ccc06352c6e5505d769be97b"
			"3b204da6ef55507aa104a3a35c5af41cf2fa364d60fd967f43e3933ba6d783d",
		"00f4bb8cc7f86db26700a7f3eceeeed3f0b5c6b5107c4da97740ab21a29906c42dbbb"
			"3e377de9f251f6b93937fa99a3248f4eafcbe95edc0f4f71be356d661f41b02"
	},
	{
		3,
		"01a73d352443de29195dd91d6a64b5959479b52a6e5b123d9ab9e5ad7a112d7a8dd1a"
			"d3f164a3a4832051da6bd16b59fe21baeb490862c32ea05a5919d2ede37ad7d",
		"013e9b03b97dfa62ddd9979f86c6cab814f2f1557fa82a9d0317d2f8ab1fa355ceec2"
			"e2dd4cf8dc575b02d5aced1dec3c70cf105c9bc93a590425f588ca1ee86c0e5"
	},
	{
		4,
		"0035b5df64ae2ac204c354b483487c9070cdc61c891c5ff39afc06c5d55541d3ceac8"
			"659e24afe3d0750e8b88e9f078af066a1d5025b08e5a5e2fbc87412871902f3",
		"0082096f84261279d2b673e0178eb0b4abb65521aef6e6e32e1b5ae63fe2f19907f27"
			"9f283e54ba385405224f750a95b85eebb7faef04699d1d9e21f47fc346e4d0d"
	},
	{
		5,
		"00652bf3c52927a432c73dbc3391c04eb0bf7a596efdb53f0d24cf03dab8f177ace43"
			"83c0c6d5e3014237112feaf137e79a329d7e1e6d8931738d5ab5096ec8f3078",
		"015be6ef1bdd6601d6ec8a2b73114a8112911cd8fe8e872e0051edd817c9a0347087b"
			"b6897c9072cf374311540211cf5ff79d1f007257354f7f8173cc3e8deb090cb"
	},
	{
		6,
		"01ee4569d6cdb59219532eff34f94480d195623d30977fd71cf3981506ade4ab01525"
			"fbcca16153f7394e0727a239531be8c2f66e95657f380ae23731bedf79206b9",
		"01de0255ad0cc64f586ae2dd270546e3b1112aabbb73da5a808e7240a926201a8a96c"
			"ab72d0e56648c9df96c984de274f2203dc7b8b55ca0dade1eaccd7858d44f17"
	},
	{
		7,
		"0056d5d1d99d5b7f6346eeb65fda0b073a0c5f22e0e8f5483228f018d2c2f7114c5d8"
			"c308d0abfc698d8c9a6df30dce3bbc46f953f50fdc2619a01cead882816ecd4",
		"003d2d1b7d9baaa2a110d1d8317a39d68478b5c582d02824f0dd71dbd98a26cbde556"
			"bd0f293cdec9e2b9523a34591ce1a5f9e76712a5ddefc7b5c6b8bc90525251b"
	},
	{
		8,
		"000822c40fb6301f7262a8348396b010e25bd4e29d8a9b003e0a8b8a3b05f826298f5"
			"bfea5b8579f49f08b598c1bc8d79e1ab56289b5a6f4040586f9ea54aa78ce68",
		"016331911d5542fc482048fdab6e78853b9a44f8ede9e2c0715b5083de610677a8f18"
			"9e9c0aa5911b4bff0ba0df065c578699f3ba940094713538ad642f11f17801c"
	},
	{
		9,
		"01585389e359e1e21826a2f5bf157156d488ed34541b988746992c4ab145b8c6b6657"
			"429e1396134da35f3c556df725a318f4f50babd85cd28661f45627967cbe207",
		"002a2e618c9a8aedf39f0b55557a27ae938e3088a654ee1cebb6c825ba263ddb446e0"
			"d69e5756057ac840ff56ecf4abfd87d736c2ae928880f343aa0ea86b9ad2a4e"
	},
	{
		10,
		"0190eb8f22bda61f281dfcfe7bb6721ec4cd901d879ac09ac7c34a9246b11ada8910a"
			"2c7c178fcc263299daa4da9842093f37c2e411f1a8e819a87ff09a04f2f3320",
		"01eb5d96b8491614ba9dbaeab3b0ca2ba760c2eeb2144251b20ba97fd78a62ef62d2b"
			"f5349d44d9864bb536f6163dc57ebeff3689639739faa172954bc98135ec759"
	},
	{
		11,
		"008a75841259fdedff546f1a39573b4315cfed5dc7ed7c17849543ef2c54f2991652f"
			"3dbc5332663da1bd19b1aebe3191085015c024fa4c9a902ecc0e02dda0cdb9a",
		"0096fb303fcbba2129849d0ca877054fb2293add566210bd0493ed2e95d4e0b9b82b1"
			"bc8a90e8b42a4ab3892331914a95336dcac80e3f4819b5d58874f92ce48c808"
	},
	{
		12,
		"01c0d9dcec93f8221c5de4fae9749c7fde1e81874157958457b6107cf7a5967713a64"
			"4e90b7c3fb81b31477fee9a60e938013774c75c530928b17be69571bf842d8c",
		"014048b5946a4927c0fe3ce1d103a682ca4763fe65ab71494da45e404abf6a17c097d"
			"6d18843d86fcdb6cc10a6f951b9b630884ba72224f5ae6c79e7b1a3281b17f0"
	},
	{
		13,
		"007e3e98f984c396ad9cd7865d2b4924861a93f736cde1b4c2384eedd2beaf5b86613"
			"2c45908e03c996a3550a5e79ab88ee94bec3b00ab38eff81887848d32fbcda7",
		"0108ee58eb6d781feda91a1926daa3ed5a08ced50a386d5421c69c7a67ae5c1e212ac"
			"1bd5d5838bc763f26dfdd351cbfbbc36199eaaf9117e9f7291a01fb022a71c9"
	},
	{
		14,
		"01875bc7dc551b1b65a9e1b8ccfaaf84ded1958b401494116a2fd4fb0babe0b319997"
			"4fc06c8b897222d79df3e4b7bc744aa6767f6b812efbf5d2c9e682dd3432d74",
		"005ca4923575dacb5bd2d66290bbabb4bdfb8470122b8e51826a0847ce9b86d7ed62d"
			"07781b1b4f3584c11e89bf1d133dc0d5b690f53a87c84be41669f852700d54a"
	},
	{
		15,
		"006b6ad89abcb92465f041558fc546d4300fb8fbcc30b40a0852d697b532df128e11b"
			"91cce27dbd00ffe7875bd1c8fc0331d9b8d96981e3f92bde9afe337bcb8db55",
		"01b468da271571391d6a7ce64d2333edbf63df0496a9bad20cba4b62106997485ed57"
			"e9062c899470a802148e2232c96c99246fd90cc446abdd956343480a1475465",
	},
	{
		19,
		"00998dcce486419c3487c0f948c2d5a1a07245b77e0755df547efff0acdb3790e7f1f"
			"a3b3096362669679232557d7a45970dfecf431e725bbde478ff0b2418d6a19b",
		"0137d5da0626a021ed5cc3942497535b245d67d28aee2b7bcf4acc50eee3654577277"
			"3ad963ff2eb8cf9b0ec39991631c377f5a4d89ea9fbfe44a9091a695bfd0575"
	},
	{
		20,
		"018bdd7f1b889598a4653deeae39cc6f8cc2bd767c2ab0d93fb12e968fbed342b5170"
			"9506339cb1049cb11dd48b9bdb3cd5cad792e43b74e16d8e2603bfb11b0344f",
		"00c5aadbe63f68ca5b6b6908296959bf0af89ee7f52b410b9444546c550952d311204"
			"da3bdddc6d4eae7edfaec1030da8ef837ccb22eee9cfc94dd3287fed0990f94"
	}
};

static int ecp_test(void *p, DrewLoader *ldr)
{
	drew_ecc_t curve;
	drew_ecc_point_t p1, pres, pcur;
	drew_bignum_t bn;
	drew_param_t param;
	const void *bnfunctbl;
	int id = 0, res = 0, len = 0;
	uint8_t buf[128];

	if ((id = drew_loader_lookup_by_name(ldr, "Bignum", 0, -1)) < 0)
		return id;
	drew_loader_get_functbl(ldr, id, &bnfunctbl);

	bn.functbl = bnfunctbl;
	bn.functbl->init(&bn, 0, ldr, NULL);

	param.name = "bignum";
	param.param.value = &bn;
	param.next = NULL;

	ecp_init(&curve, 0, ldr, &param);
	ecp_setcurvename(&curve, "secp521r1");
	ecp_point(&curve, &p1);
	ecp_point(&curve, &pres);
	ecp_point(&curve, &pcur);

	ecpt_setinf(&p1, false);
	len = strtobytes(buf, sizeof(buf), p521_testcases[0].x);
	ecpt_setcoordbytes(&p1, buf, len, DREW_ECC_POINT_X);
	len = strtobytes(buf, sizeof(buf), p521_testcases[0].y);
	ecpt_setcoordbytes(&p1, buf, len, DREW_ECC_POINT_Y);

	for (size_t i = 0; i < DIM(p521_testcases); i++) {
		res <<= 1;

		ecpt_setinf(&pcur, false);
		len = strtobytes(buf, sizeof(buf), p521_testcases[i].x);
		ecpt_setcoordbytes(&pcur, buf, len, DREW_ECC_POINT_X);
		len = strtobytes(buf, sizeof(buf), p521_testcases[i].y);
		ecpt_setcoordbytes(&pcur, buf, len, DREW_ECC_POINT_Y);

		bn.functbl->setsmall(&bn, p521_testcases[i].k);
		ecpt_mul(&pres, &p1, &bn);

		res |= !!ecpt_compare(&pres, &pcur);
	}

	ecp_fini(&curve, 0);

	bn.functbl->fini(&bn, 0);

	return res;
}

static int ecpt_info(int op, void *p)
{
	return -DREW_ERR_INVALID;
}

static int ecpt_info2(const drew_ecc_point_t *ctx, int op, drew_param_t *out,
		const drew_param_t *in)
{
	return -DREW_ERR_INVALID;
}

static int ecpt_init(drew_ecc_point_t *ctx, int flags,
		DrewLoader *ldr, const drew_param_t *param)
{
	const drew_ecc_t *curvectx = NULL;
	struct point *pt;
	struct curve *curve;

	for (const drew_param_t *p = param; p; p = p->next)
		if (!strcmp(p->name, "curve"))
			curvectx = param->param.value;

	if (!curvectx)
		return -DREW_ERR_MORE_INFO;

	if (!(flags & DREW_ECC_FIXED))
		if (!(ctx->ctx = drew_mem_malloc(sizeof(struct point))))
			return -ENOMEM;

	ctx->functbl = &ecpt_functbl;
	pt = ctx->ctx;
	pt->curve = curve = curvectx->ctx;
	curve->p.functbl->clone(&pt->x, &curve->p, 0);
	curve->p.functbl->clone(&pt->y, &curve->p, 0);
	pt->inf = true;

	return 0;
}

static int ecpt_clone(drew_ecc_point_t *new, const drew_ecc_point_t *old,
		int flags)
{
	struct point *pn, *po = old->ctx;

	if (!(flags & DREW_ECC_FIXED))
		if (!(new->ctx = drew_mem_malloc(sizeof(struct point))))
			return -ENOMEM;

	new->functbl = &ecpt_functbl;
	pn = new->ctx;
	pn->curve = po->curve;
	po->x.functbl->clone(&pn->x, &po->x, 0);
	po->y.functbl->clone(&pn->y, &po->y, 0);
	pn->inf = po->inf;
	return 0;
}

static int ecpt_fini(drew_ecc_point_t *ctx, int flags)
{
	struct point *pt = ctx->ctx;

	pt->x.functbl->fini(&pt->x, 0);
	pt->y.functbl->fini(&pt->y, 0);

	if (!(flags & DREW_ECC_FIXED)) {
		drew_mem_free(pt);
		ctx->ctx = NULL;
	}

	return 0;
}

static int ecpt_setinf(drew_ecc_point_t *ctx, bool val)
{
	struct point *pt = ctx->ctx;
	pt->inf = val;

	return 0;
}

static int ecpt_isinf(const drew_ecc_point_t *ctx)
{
	struct point *pt = ctx->ctx;
	return pt->inf;
}

static int ecpt_compare(const drew_ecc_point_t *a, const drew_ecc_point_t *b)
{
	struct point *pta = a->ctx, *ptb = b->ctx;
	int val;

	if (pta->inf && ptb->inf)
		return 0;
	if (pta->inf && !ptb->inf)
		return -1;
	if (!pta->inf && ptb->inf)
		return 1;
	val = pta->x.functbl->compare(&pta->x, &ptb->x, 0);
	return val ? val : pta->y.functbl->compare(&pta->y, &ptb->y, 0);
}

static int ecpt_setcoordbytes(drew_ecc_point_t *ctx, const uint8_t *data,
		size_t len, int coord)
{
	struct point *pt = ctx->ctx;

	if (coord == DREW_ECC_POINT_X)
		return pt->x.functbl->setbytes(&pt->x, data, len);
	if (coord == DREW_ECC_POINT_Y)
		return pt->y.functbl->setbytes(&pt->y, data, len);
	if (coord == DREW_ECC_POINT_SEC)
		return load_point(pt, data, len);
	return -DREW_ERR_INVALID;
}

static int ecpt_coordbytes(const drew_ecc_point_t *ctx, uint8_t *data,
		size_t len, int coord)
{
	struct point *pt = ctx->ctx;

	if (coord == DREW_ECC_POINT_X)
		return pt->x.functbl->bytes(&pt->x, data, len);
	if (coord == DREW_ECC_POINT_Y)
		return pt->y.functbl->bytes(&pt->y, data, len);
	if (coord == DREW_ECC_POINT_SEC)
		return store_point(pt, data, len);
	if (coord == DREW_ECC_POINT_SEC_COMPRESSED)
		return -DREW_ERR_NOT_IMPL;
	return -DREW_ERR_INVALID;
}

static int ecpt_ncoordbytes(const drew_ecc_point_t *ctx, int coord)
{
	struct point *pt = ctx->ctx;

	if (coord == DREW_ECC_POINT_X)
		return pt->x.functbl->nbytes(&pt->x);
	if (coord == DREW_ECC_POINT_Y)
		return pt->y.functbl->nbytes(&pt->y);
	if (coord == DREW_ECC_POINT_SEC) {
		if (pt->inf)
			return 1;
		else
			return (pt->curve->p.functbl->nbytes(&pt->curve->p) * 2) + 1;
	}
	if (coord == DREW_ECC_POINT_SEC_COMPRESSED)
		return -DREW_ERR_NOT_IMPL;
	return -DREW_ERR_INVALID;
}


static int ecpt_setcoordbignum(drew_ecc_point_t *ctx, const drew_bignum_t *data,
		int coord)
{
	struct point *pt = ctx->ctx;

	if (coord == DREW_ECC_POINT_X)
		return pt->x.functbl->clone(&pt->x, data, DREW_ECC_FIXED);
	if (coord == DREW_ECC_POINT_Y)
		return pt->y.functbl->clone(&pt->y, data, DREW_ECC_FIXED);
	return -DREW_ERR_INVALID;
}

static int ecpt_coordbignum(const drew_ecc_point_t *ctx,
		drew_bignum_t *bn, int coord)
{
	struct point *pt = ctx->ctx;

	if (coord == DREW_ECC_POINT_X)
		return pt->x.functbl->clone(bn, &pt->x, DREW_ECC_FIXED);
	if (coord == DREW_ECC_POINT_Y)
		return pt->y.functbl->clone(bn, &pt->y, DREW_ECC_FIXED);
	return -DREW_ERR_INVALID;
}

static int ecpt_inv(drew_ecc_point_t *r, const drew_ecc_point_t *a)
{
	struct point *ptr = r->ctx, *pta = a->ctx;

	ptr->inf = pta->inf;
	ptr->curve = pta->curve;
	if (pta->inf)
		return 0;
	ptr->x.functbl->clone(&ptr->x, &pta->x, DREW_ECC_FIXED);
	ptr->y.functbl->invmod(&ptr->y, &pta->y, &ptr->curve->p);
	return 0;
}

static int ecpt_dbl(drew_ecc_point_t *ptr, const drew_ecc_point_t *pta)
{
	struct point *r = ptr->ctx, *a = pta->ctx;
	const drew_bignum_functbl_t *ft = r->y.functbl;

	r->curve = a->curve;
	if (a->inf)
		r->inf = true;
	else {
		drew_bignum_t lambda, t1, three, two, x, y;
		const drew_bignum_t *p = &r->curve->p;
		ft->clone(&three, &r->x, 0);
		ft->setsmall(&three, 3);
		ft->clone(&two, &three, 0);
		ft->setsmall(&two, 2);
		ft->clone(&x, &two, 0);
		ft->clone(&y, &two, 0);
		// lambda = (3(x1^2) + a) / 2(y1)
		ft->clone(&lambda, &a->x, 0);
		ft->squaremod(&lambda, &lambda, p);
		ft->mulmod(&lambda, &lambda, &three, p);
		ft->add(&lambda, &lambda, &r->curve->a);
		ft->clone(&t1, &a->y, 0);
		ft->mul(&t1, &t1, &two);
		ft->invmod(&t1, &t1, p);
		ft->mul(&lambda, &lambda, &t1);
		ft->mod(&lambda, &lambda, p);
		// x = lambda^2 - 2(x1)
		ft->squaremod(&x, &lambda, p);
		ft->sub(&x, &x, &a->x);
		ft->sub(&x, &x, &a->x);
		ft->mod(&x, &x, p);
		// y = lambda(x1 - x) - y1
		ft->sub(&y, &a->x, &x);
		ft->mulmod(&y, &y, &lambda, p);
		ft->sub(&y, &y, &a->y);
		ft->mod(&r->y, &y, p);
		// This is needed to allow a and r to be the same.
		ft->clone(&r->x, &x, DREW_ECC_FIXED);
		ft->fini(&lambda, 0);
		ft->fini(&t1, 0);
		ft->fini(&three, 0);
		ft->fini(&two, 0);
		ft->fini(&x, 0);
		ft->fini(&y, 0);
		r->inf = false;
	}
	return 0;
}

static int ecpt_add(drew_ecc_point_t *ptr, const drew_ecc_point_t *pta,
		const drew_ecc_point_t *ptb)
{
	struct point *r = ptr->ctx, *a = pta->ctx, *b = ptb->ctx;
	const drew_bignum_functbl_t *ft = r->y.functbl;

	r->curve = a->curve;
	if (a->inf && b->inf)
		r->inf = true;
	else if (a->inf) {
		r->inf = false;
		ft->clone(&r->x, &b->x, DREW_ECC_FIXED);
		ft->clone(&r->y, &b->y, DREW_ECC_FIXED);
	}
	else if (b->inf) {
		r->inf = false;
		ft->clone(&r->x, &a->x, DREW_ECC_FIXED);
		ft->clone(&r->y, &a->y, DREW_ECC_FIXED);
	}
	else if (!pta->functbl->compare(pta, ptb))
		return ecpt_dbl(ptr, pta);
	else if (!ft->compare(&a->x, &b->x, 0))
		r->inf = true;
	else {
		drew_bignum_t lambda, t1, x, y;
		const drew_bignum_t *p = &r->curve->p;
		r->inf = false;
		// lambda = (y2 - y1) / (x2 - x1) mod p.
		ft->clone(&lambda, &b->y, 0);
		ft->clone(&t1, &b->x, 0);
		ft->clone(&x, &b->x, 0);
		ft->clone(&y, &b->x, 0);
		ft->sub(&lambda, &lambda, &a->y);
		ft->sub(&t1, &t1, &a->x);
		ft->invmod(&t1, &t1, p);
		ft->abs(&t1, &t1);
		ft->mul(&lambda, &lambda, &t1);
		ft->mod(&lambda, &lambda, p);
		// x = lambda^2 - x1 - x2
		ft->squaremod(&x, &lambda, p);
		ft->sub(&x, &x, &a->x);
		ft->sub(&x, &x, &b->x);
		ft->mod(&x, &x, p);
		// y = lambda(x1 - x) - y1
		ft->sub(&y, &a->x, &x);
		ft->mulmod(&y, &y, &lambda, p);
		ft->sub(&y, &y, &a->y);
		ft->mod(&r->y, &y, p);
		ft->clone(&r->x, &x, DREW_ECC_FIXED);
		ft->fini(&lambda, 0);
		ft->fini(&t1, 0);
		ft->fini(&x, 0);
		ft->fini(&y, 0);
	}
	return 0;
}

// This uses Shamir's Trick.  The implementation is from Bouncy Castle.
static int ecpt_mul2(drew_ecc_point_t *res, const drew_ecc_point_t *ptp,
		const drew_bignum_t *a, const drew_ecc_point_t *ptq,
		const drew_bignum_t *b)
{
	drew_ecc_point_t z, tmp, *r = &tmp;
	size_t abits = a->functbl->nbits(a);
	size_t bbits = b ? b->functbl->nbits(b) : 0;
	size_t nbits = MAX(abits, bbits);

	if (!!ptq != !!b)
		return -DREW_ERR_INVALID;

	ecpt_clone(&z, ptp, 0);
	if (b)
		ecpt_add(&z, ptp, ptq);
	ecpt_clone(r, ptp, 0);
	ecpt_setinf(r, true);

	for (int i = nbits - 1; i >= 0; i--) {
		ecpt_dbl(r, r);
		if (a->functbl->getbit(a, i)) {
			if (b && b->functbl->getbit(b, i))
				ecpt_add(r, r, &z);
			else
				ecpt_add(r, r, ptp);
		}
		else if (b && b->functbl->getbit(b, i))
			ecpt_add(r, r, ptq);
	}

	ecpt_fini(&z, 0);
	ecpt_fini(res, 0);
	ecpt_clone(res, r, 0);
	return 0;
}

static int ecpt_mul(drew_ecc_point_t *ptr, const drew_ecc_point_t *pta,
		const drew_bignum_t *b)
{
	return ecpt_mul2(ptr, pta, b, NULL, NULL);
}

static int ecpt_test(void *p, DrewLoader *ldr)
{
	return -DREW_ERR_NOT_IMPL;
}

struct plugin {
	const char *name;
	const drew_ecc_functbl_t *functbl;
};

static struct plugin plugin_data[] = {
	{ "EllipticCurvePrime", &ecp_functbl }
};

EXPORT()
int DREW_PLUGIN_NAME(prime)(void *ldr, int op, int id, void *p)
{
	int nplugins = sizeof(plugin_data)/sizeof(plugin_data[0]);

	if (id < 0 || id >= nplugins)
		return -DREW_ERR_INVALID;

	switch (op) {
		case DREW_LOADER_LOOKUP_NAME:
			return 0;
		case DREW_LOADER_GET_NPLUGINS:
			return nplugins;
		case DREW_LOADER_GET_TYPE:
			return DREW_TYPE_ECC;
		case DREW_LOADER_GET_FUNCTBL_SIZE:
			return sizeof(drew_ecc_functbl_t);
		case DREW_LOADER_GET_FUNCTBL:
			memcpy(p, plugin_data[id].functbl, sizeof(drew_ecc_functbl_t));
			return 0;
		case DREW_LOADER_GET_NAME_SIZE:
			return strlen(plugin_data[id].name) + 1;
		case DREW_LOADER_GET_NAME:
			memcpy(p, plugin_data[id].name, strlen(plugin_data[id].name)+1);
			return 0;
		default:
			return -DREW_ERR_INVALID;
	}
}
UNEXPORT()
