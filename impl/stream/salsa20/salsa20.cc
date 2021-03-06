/*-
 * Copyright © 2011 brian m. carlson
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
#include <utility>

#include <stdio.h>
#include <string.h>

#include <internal.h>
#include <drew/drew.h>
#include <drew/plugin.h>
#include <drew/stream.h>
#include "salsa20.hh"
#include "stream-plugin.h"
#include "testcase.hh"

HIDE()
extern "C" {

#ifdef SALSA_HAVE_ASM
typedef drew::Salsa20AssemblerKeystream::AlignedData salsa_ctx_t;

void salsa_asm_encrypt_bytes(salsa_ctx_t *, const uint8_t *pt, uint8_t *ct,
		uint32_t msglen);
void salsa_asm_keysetup(salsa_ctx_t *, const uint8_t *key, uint32_t keysz,
		uint32_t ivsize);
void salsa_asm_ivsetup(salsa_ctx_t *, const uint8_t *key);

static int salsa_asm_test(void *, const drew_loader_t *);
static int salsa_asm_init(drew_stream_t *ctx, int flags, const drew_loader_t *,
		const drew_param_t *);
#endif

static int salsa20_test(void *, const drew_loader_t *);
static int salsa20_info(int op, void *p);
static int salsa20_info2(const drew_stream_t *ctx, int op, drew_param_t *out,
		const drew_param_t *in);
static int salsa20_init(drew_stream_t *ctx, int flags, const drew_loader_t *,
		const drew_param_t *);
static int salsa20_clone(drew_stream_t *newctx, const drew_stream_t *oldctx,
		int flags);
static int salsa20_reset(drew_stream_t *ctx);
static int salsa20_setiv(drew_stream_t *ctx, const uint8_t *key, size_t len);
static int salsa20_setkey(drew_stream_t *ctx, const uint8_t *key, size_t len,
		int mode);
static int salsa20_encrypt(drew_stream_t *ctx, uint8_t *out, const uint8_t *in,
		size_t len);
static int salsa20_encryptfast(drew_stream_t *ctx, uint8_t *out,
		const uint8_t *in, size_t len);
static int salsa20_fini(drew_stream_t *ctx, int flags);

PLUGIN_FUNCTBL(salsa20, salsa20_info, salsa20_info2, salsa20_init, salsa20_setiv, salsa20_setkey, salsa20_encrypt, salsa20_encrypt, salsa20_encryptfast, salsa20_encryptfast, salsa20_test, salsa20_fini, salsa20_clone, salsa20_reset);

#ifdef SALSA_HAVE_ASM
PLUGIN_FUNCTBL(salsa_asm, salsa20_info, salsa20_info2, salsa_asm_init, salsa20_setiv, salsa20_setkey, salsa20_encrypt, salsa20_encrypt, salsa20_encryptfast, salsa20_encryptfast, salsa_asm_test, salsa20_fini, salsa20_clone, salsa20_reset);
#endif

static int salsa20_maintenance_test(void)
{
	using namespace drew;

	int res = 0;

	res |= StreamTestCase<Salsa20>::MaintenanceTest("9bb285b4046d23d2afa572193"
			"95503a43940b5107670d26614b938c5d29a7ebbb5555be6a22ea75605078d974d"
			"c8e4e768e4ef612d32239afc8fb19a7508b5b5253708fc5f6e45f8fd72d5b317b"
			"89b1ab552eb337d4841cc37da6182610ed7848b596fcc5824a8330318f05013c7"
			"1f65094d67afeebc4286d976055d95a0397b", 16, 8);
	res <<= 4;
	res |= StreamTestCase<Salsa20>::MaintenanceTest("c183db6a97d3aa10045cd7847"
			"a7b43d581c11c2f82212481d44cb3fda40c70de2830a7b01a04c54d87afc93da9"
			"a6a0d4dd8b4b60dbf0773ab4f76b8c63b64205b4cc996aa2c1ba9c11b897d1630"
			"a3ad72285edb94ced28357587063054929515f9e795e9b82f66a22384724b06e3"
			"0203a05a107ec6379adf0409d1d917b17a47", 32, 8);

	return res;
}

static int salsa20_test(void *, const drew_loader_t *)
{
	using namespace drew;

	int res = 0;
	res |= salsa20_maintenance_test();

	return res;
}

static const int salsa_keysz[] = {16, 32};
static const int salsa_ivsz[] = {8};

static int salsa20_info(int op, void *p)
{
	switch (op) {
		case DREW_STREAM_VERSION:
			return CURRENT_ABI;
		case DREW_STREAM_KEYSIZE:
			for (size_t i = 0; i < DIM(salsa_keysz); i++) {
				const int *x = reinterpret_cast<int *>(p);
				if (salsa_keysz[i] > *x)
					return salsa_keysz[i];
			}
			return 0;
		case DREW_STREAM_INTSIZE:
			return sizeof(drew::Salsa20);
		case DREW_STREAM_BLKSIZE:
			return 64;
		default:
			return -DREW_ERR_INVALID;
	}
}

static int salsa20_info2(const drew_stream_t *ctx, int op, drew_param_t *out,
		const drew_param_t *in)
{
	switch (op) {
		case DREW_STREAM_VERSION:
			return CURRENT_ABI;
		case DREW_STREAM_KEYSIZE_LIST:
			for (drew_param_t *p = out; p; p = p->next)
				if (!strcmp(p->name, "keySize")) {
					p->param.array.ptr = (void *)salsa_keysz;
					p->param.array.len = DIM(salsa_keysz);
				}
			return 0;
		case DREW_STREAM_KEYSIZE_CTX:
			if (ctx && ctx->ctx) {
				const drew::Salsa20 *algo = (const drew::Salsa20 *)ctx->ctx;
				return algo->GetKeySize();
			}
			return -DREW_ERR_MORE_INFO;
		case DREW_STREAM_IVSIZE_LIST:
			for (drew_param_t *p = out; p; p = p->next)
				if (!strcmp(p->name, "ivSize")) {
					p->param.array.ptr = (void *)salsa_ivsz;
					p->param.array.len = DIM(salsa_ivsz);
				}
			return 0;
		case DREW_STREAM_IVSIZE_CTX:
			return 8;
		case DREW_STREAM_INTSIZE:
			return sizeof(drew::Salsa20);
		case DREW_STREAM_BLKSIZE:
			return 64;
		default:
			return -DREW_ERR_INVALID;
	}
}

static int salsa20_init(drew_stream_t *ctx, int flags, const drew_loader_t *,
		const drew_param_t *param)
{
	drew::Salsa20 *p;
	size_t rounds = 20;

	for (const drew_param_t *pp = param; pp; pp = pp->next) {
		if (!strcmp(pp->name, "rounds"))
			rounds = pp->param.number;
	}

	rounds /= 2;

	if (flags & DREW_STREAM_FIXED)
		p = new (ctx->ctx) drew::Salsa20(rounds);
	else
		p = new drew::Salsa20(rounds);
	ctx->ctx = p;
	ctx->functbl = &salsa20functbl;
	return 0;
}

static int salsa20_clone(drew_stream_t *newctx, const drew_stream_t *oldctx,
		int flags)
{
	drew::Salsa20 *p;
	const drew::Salsa20 *q = reinterpret_cast<drew::Salsa20 *>(oldctx->ctx);
	if (flags & DREW_STREAM_FIXED)
		p = new (newctx->ctx) drew::Salsa20(*q);
	else
		p = new drew::Salsa20(*q);
	newctx->ctx = p;
	newctx->functbl = oldctx->functbl;
	return 0;
}

static int salsa20_reset(drew_stream_t *ctx)
{
	drew::Salsa20 *p = reinterpret_cast<drew::Salsa20 *>(ctx->ctx);
	p->Reset();
	return 0;
}

static int salsa20_setiv(drew_stream_t *ctx, const uint8_t *key, size_t len)
{
	drew::Salsa20 *p = reinterpret_cast<drew::Salsa20 *>(ctx->ctx);
	p->SetNonce(key, len);
	return 0;
}

static int salsa20_setkey(drew_stream_t *ctx, const uint8_t *key, size_t len,
		int mode)
{
	drew::Salsa20 *p = reinterpret_cast<drew::Salsa20 *>(ctx->ctx);
	p->SetKey(key, len);
	return 0;
}

static int salsa20_encrypt(drew_stream_t *ctx, uint8_t *out, const uint8_t *in,
		size_t len)
{
	drew::Salsa20 *p = reinterpret_cast<drew::Salsa20 *>(ctx->ctx);
	p->Encrypt(out, in, len);
	return 0;
}

static int salsa20_encryptfast(drew_stream_t *ctx, uint8_t *out,
		const uint8_t *in, size_t len)
{
	drew::Salsa20 *p = reinterpret_cast<drew::Salsa20 *>(ctx->ctx);
	p->EncryptFast(out, in, len);
	return 0;
}

static int salsa20_fini(drew_stream_t *ctx, int flags)
{
	drew::Salsa20 *p = reinterpret_cast<drew::Salsa20 *>(ctx->ctx);
	if (flags & DREW_STREAM_FIXED)
		p->~Salsa20();
	else
		delete p;
	return 0;
}

#ifdef SALSA_HAVE_ASM
static int salsa_asm_init(drew_stream_t *ctx, int flags, const drew_loader_t *,
		const drew_param_t *param)
{
	drew::Salsa20 *p;
	size_t rounds = 20;

	for (const drew_param_t *pp = param; pp; pp = pp->next) {
		if (!strcmp(pp->name, "rounds"))
			rounds = pp->param.number;
	}

	if (rounds != 20)
		return -DREW_ERR_INVALID;

	if (flags & DREW_STREAM_FIXED)
		p = new (ctx->ctx) drew::Salsa20(new drew::Salsa20AssemblerKeystream);
	else
		p = new drew::Salsa20(new drew::Salsa20AssemblerKeystream);
	ctx->ctx = p;
	ctx->functbl = &salsa_asmfunctbl;
	return 0;
}

static int salsa_asm_test(void *, const drew_loader_t *)
{
	return -DREW_ERR_NOT_IMPL;
}
#endif

PLUGIN_DATA_START()
#ifdef SALSA_HAVE_ASM
PLUGIN_DATA(salsa_asm, "Salsa20")
#endif
PLUGIN_DATA(salsa20, "Salsa20")
PLUGIN_DATA_END()
PLUGIN_INTERFACE(salsa20)

}

drew::Salsa20::Salsa20() : m_ks(new Salsa20Keystream)
{
	m_ks->SetRounds(10);
}

drew::Salsa20::Salsa20(size_t nrounds) : m_ks(new Salsa20Keystream)
{
	m_ks->SetRounds(nrounds);
}

drew::Salsa20::Salsa20(Salsa20GenericKeystream *ks) : m_ks(ks)
{
	m_ks->SetRounds(10);
}

drew::Salsa20::Salsa20(Salsa20GenericKeystream *ks, size_t nrounds) : m_ks(ks)
{
	m_ks->SetRounds(nrounds);
}

void drew::Salsa20::Reset()
{
	m_ks->Reset();
	m_nbytes = 0;
}

void drew::Salsa20::SetKey(const uint8_t *key, size_t sz)
{
	m_ks->Reset();
	m_ks->SetKey(key, sz);
	m_nbytes = 0;
}

void drew::Salsa20::SetNonce(const uint8_t *iv, size_t sz)
{
	m_ks->SetNonce(iv, sz);
}

void drew::Salsa20::EncryptFast(uint8_t *out, const uint8_t *in, size_t len)
{
	CopyAndXorAligned(out, in, len, m_buf, sizeof(m_buf), *m_ks);
}

void drew::Salsa20::Encrypt(uint8_t *out, const uint8_t *in, size_t len)
{
	CopyAndXor(out, in, len, m_buf, sizeof(m_buf), m_nbytes, *m_ks);
}

void drew::Salsa20::Decrypt(uint8_t *out, const uint8_t *in, size_t len)
{
	return Encrypt(out, in, len);
}

typedef drew::Salsa20Keystream::endian_t E;

drew::Salsa20Keystream::Salsa20Keystream()
{
	Reset();
	ctr = 0;
}

void drew::Salsa20Keystream::SetRounds(size_t rounds)
{
	nrounds = rounds;
}

void drew::Salsa20Keystream::SetKey(const uint8_t *key, size_t sz)
{
	keysz = sz;
	E::Copy(state.buf+1, key, 16);
	if (sz == 16)
		E::Copy(state.buf+11, key, 16);
	else // sz == 32
		E::Copy(state.buf+11, key+16, 16);
}

void drew::Salsa20Keystream::SetNonce(const uint8_t *iv, size_t sz)
{
	E::Copy(state.buf+6, iv, sz);

	state.buf[ 0] = 0x61707865;
	state.buf[ 5] = (keysz == 16) ? 0x3120646e : 0x3320646e;
	state.buf[10] = (keysz == 16) ? 0x79622d36 : 0x79622d32;
	state.buf[15] = 0x6b206574;
}

void drew::Salsa20Keystream::Reset()
{
	ctr = 0;
}

inline void drew::Salsa20Keystream::DoHash(AlignedData &cur)
{
	const AlignedData &st = state;
	memcpy(cur.buf, st.buf, 16 * sizeof(uint32_t));

	for (size_t i = 0; i < nrounds; i++) {
		cur.buf[ 4] ^= RotateLeft(cur.buf[ 0] + cur.buf[12],  7);
		cur.buf[ 8] ^= RotateLeft(cur.buf[ 4] + cur.buf[ 0],  9);
		cur.buf[12] ^= RotateLeft(cur.buf[ 8] + cur.buf[ 4], 13);
		cur.buf[ 0] ^= RotateLeft(cur.buf[12] + cur.buf[ 8], 18);
		cur.buf[ 9] ^= RotateLeft(cur.buf[ 5] + cur.buf[ 1],  7);
		cur.buf[13] ^= RotateLeft(cur.buf[ 9] + cur.buf[ 5],  9);
		cur.buf[ 1] ^= RotateLeft(cur.buf[13] + cur.buf[ 9], 13);
		cur.buf[ 5] ^= RotateLeft(cur.buf[ 1] + cur.buf[13], 18);
		cur.buf[14] ^= RotateLeft(cur.buf[10] + cur.buf[ 6],  7);
		cur.buf[ 2] ^= RotateLeft(cur.buf[14] + cur.buf[10],  9);
		cur.buf[ 6] ^= RotateLeft(cur.buf[ 2] + cur.buf[14], 13);
		cur.buf[10] ^= RotateLeft(cur.buf[ 6] + cur.buf[ 2], 18);
		cur.buf[ 3] ^= RotateLeft(cur.buf[15] + cur.buf[11],  7);
		cur.buf[ 7] ^= RotateLeft(cur.buf[ 3] + cur.buf[15],  9);
		cur.buf[11] ^= RotateLeft(cur.buf[ 7] + cur.buf[ 3], 13);
		cur.buf[15] ^= RotateLeft(cur.buf[11] + cur.buf[ 7], 18);

		cur.buf[ 1] ^= RotateLeft(cur.buf[ 0] + cur.buf[ 3],  7);
		cur.buf[ 2] ^= RotateLeft(cur.buf[ 1] + cur.buf[ 0],  9);
		cur.buf[ 3] ^= RotateLeft(cur.buf[ 2] + cur.buf[ 1], 13);
		cur.buf[ 0] ^= RotateLeft(cur.buf[ 3] + cur.buf[ 2], 18);
		cur.buf[ 6] ^= RotateLeft(cur.buf[ 5] + cur.buf[ 4],  7);
		cur.buf[ 7] ^= RotateLeft(cur.buf[ 6] + cur.buf[ 5],  9);
		cur.buf[ 4] ^= RotateLeft(cur.buf[ 7] + cur.buf[ 6], 13);
		cur.buf[ 5] ^= RotateLeft(cur.buf[ 4] + cur.buf[ 7], 18);
		cur.buf[11] ^= RotateLeft(cur.buf[10] + cur.buf[ 9],  7);
		cur.buf[ 8] ^= RotateLeft(cur.buf[11] + cur.buf[10],  9);
		cur.buf[ 9] ^= RotateLeft(cur.buf[ 8] + cur.buf[11], 13);
		cur.buf[10] ^= RotateLeft(cur.buf[ 9] + cur.buf[ 8], 18);
		cur.buf[12] ^= RotateLeft(cur.buf[15] + cur.buf[14],  7);
		cur.buf[13] ^= RotateLeft(cur.buf[12] + cur.buf[15],  9);
		cur.buf[14] ^= RotateLeft(cur.buf[13] + cur.buf[12], 13);
		cur.buf[15] ^= RotateLeft(cur.buf[14] + cur.buf[13], 18);
	}
	for (size_t i = 0; i < 16; i++)
		cur.buf[i] += st.buf[i];
}

void drew::Salsa20Keystream::FillBuffer(uint8_t buf[64])
{
	AlignedData cur;

	state.buf[8] = uint32_t(ctr);
	state.buf[9] = ctr >> 32;

	DoHash(cur);
	ctr++;
	E::Copy(buf, cur.buf, sizeof(cur.buf));
}

void drew::Salsa20Keystream::FillBufferAligned(uint8_t bufp[64])
{
	AlignedData cur;
	struct AlignedBytes
	{
		uint8_t buf[64] ALIGNED_T;
	};

	state.buf[8] = uint32_t(ctr);
	state.buf[9] = ctr >> 32;

	if (E::GetEndianness() == NativeEndian::GetEndianness()) {
		AlignedData *buf = reinterpret_cast<AlignedData *>(bufp);
		DoHash(*buf);
	}
	else {
		AlignedBytes *buf = reinterpret_cast<AlignedBytes *>(bufp);
		DoHash(cur);
		E::Copy(buf->buf, cur.buf, sizeof(cur.buf));
	}
	ctr++;
}

#ifdef SALSA_HAVE_ASM
drew::Salsa20AssemblerKeystream::Salsa20AssemblerKeystream()
{
}

void drew::Salsa20AssemblerKeystream::SetKey(const uint8_t *key, size_t sz)
{
	salsa_asm_keysetup(&state, key, sz*8, 64);
}

void drew::Salsa20AssemblerKeystream::SetNonce(const uint8_t *iv, size_t sz)
{
	salsa_asm_ivsetup(&state, iv);
}

void drew::Salsa20AssemblerKeystream::Reset()
{
	// FIXME: implement.
}

void drew::Salsa20AssemblerKeystream::FillBuffer(uint8_t buf[64])
{
	memset(buf, 0, 64);
	salsa_asm_encrypt_bytes(&state, buf, buf, 64);
}

void drew::Salsa20AssemblerKeystream::FillBufferAligned(uint8_t bufp[64])
{
	memset(bufp, 0, 64);
	salsa_asm_encrypt_bytes(&state, bufp, bufp, 64);
}
#endif
UNHIDE()
