#ifndef DREW_MAC_INTERFACE_H
#define DREW_MAC_INTERFACE_H

#include <errno.h>
#include <stdint.h>

#include "param.h"
#include "plugin.h"

/* The ABI version of the hash interface. */
#define DREW_MAC_VERSION 0 /* Not implemented. */
/* The length of the final MAC in bytes. */
#define DREW_MAC_SIZE 1
/* The size of the block in bytes. */
#define DREW_MAC_BLKSIZE 2
/* The endianness of this hash algorithm.  4321 is big-endian and 1234 is
 * little-endian.
 */
#define DREW_MAC_ENDIAN 3 /* Not implemented. */
/* The size of the underlying implementation's context.  This is useful for the
 * clone function if there's a need to copy the actual context into a given
 * block of memory, such as locked memory.
 */
#define DREW_MAC_INTSIZE 4

/* This bit is a flag indicating that the new context should be copied into
 * already-existing memory at *newctx.
 */
#define DREW_MAC_INIT_FIXED  1
#define DREW_MAC_CLONE_FIXED 1
/* This bit is a flag indicating that the memory storing the context should not
 * be freed because the context was created with INIT_FIXED or CLONE_FIXED.
 */
#define DREW_MAC_FINI_NO_DEALLOC 1

typedef struct {
	int (*info)(int op, void *p);
	int (*init)(void **, void *, int, drew_loader_t *, const drew_param_t *);
	int (*clone)(void **, void *, int);
	int (*fini)(void **, int);
	int (*setkey)(void *, const uint8_t *, size_t);
	int (*update)(void *, const uint8_t *, size_t);
	int (*final)(void *, uint8_t *, int);
	int (*test)(void *, drew_loader_t *);
} drew_mac_functbl0_t;
typedef drew_mac_functbl0_t drew_mac_functbl1_t;
typedef drew_mac_functbl1_t drew_mac_functbl_t;

#endif
