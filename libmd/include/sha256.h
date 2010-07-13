/*-
 * brian m. carlson <sandals@crustytoothpaste.ath.cx> wrote this source code.
 * This source code is in the public domain; you may do whatever you please with
 * it.  However, a credit in the documentation, although not required, would be
 * appreciated.
 */
/* This code implements the SHA256 and SHA224 message digest algorithms.  It is
 * compatible with OpenBSD's implementation.  The size of the SHA256_CTX struct
 * is not guaranteed compatible, however.  This implementation requires ANSI C.
 */

#ifndef BMC_SHA256_H
#define BMC_SHA256_H

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "hash.h"

#define SHA256_DIGEST_LENGTH 32
#define SHA256_DIGEST_STRING_LENGTH (SHA256_DIGEST_LENGTH*2+1)
#define SHA256_BLOCK_LENGTH 64

typedef hash_ctx_t SHA256_CTX;

void SHA256Init(SHA256_CTX *ctx);
void SHA256Update(SHA256_CTX *ctx, const uint8_t *data, size_t len);
void SHA256Pad(SHA256_CTX *ctx);
void SHA256Final(uint8_t digest[SHA256_DIGEST_LENGTH], SHA256_CTX *ctx);
void SHA256Transform(uint32_t *state, const uint8_t block[SHA256_BLOCK_LENGTH]);

char *SHA256End(SHA256_CTX *ctx, char *buf);
char *SHA256File(const char *filename, char *buf);
char *SHA256FileChunk(const char *filename, char *buf, off_t off, off_t len);
char *SHA256Data(const uint8_t *data, size_t len, char *buf);

#endif