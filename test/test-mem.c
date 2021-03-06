/*-
 * brian m. carlson <sandals@crustytoothpaste.net> wrote this source code.
 * This source code is in the public domain; you may do whatever you please with
 * it.  However, a credit in the documentation, although not required, would be
 * appreciated.
 */
#include <stdint.h>
#include <stdlib.h>

#include <drew/mem.h>

#define NCHUNKS 4096

int main(void)
{
	uint32_t buf[50], *p[NCHUNKS];

	for (int i = 0, j = 5; i < 50; i++, j += i)
		buf[i] = j *= i;

	for (int i = 0; i < NCHUNKS; i++)
		p[i] = drew_mem_memdup(buf, sizeof(buf));
	for (int i = 0; i < (NCHUNKS-1); i++) {
		// idea from strfry.
		int j = rand();
		j = j % (NCHUNKS - i) + i;
		void *t = p[i];
		p[i] = p[j];
		p[j] = t;
	}
	for (int i = 0; i < NCHUNKS; i++)
		drew_mem_free(p[i]);
	return 0;
}
