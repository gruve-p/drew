#include "plugin.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void errprint(int val, int retval)
{
	val = -val;
	fprintf(stderr, "Error %d (%s) at stage %d\n", val, strerror(val), retval);
	exit(retval);
}

int main(int argc, char **argv)
{
	drew_loader_t *ldr = NULL;
	int retval = 0;
	int size = 0;
	int id = 0;
	int type = 0, nplugins = 0;
	const void *tbl = NULL;

	if (argc < 2)
		return 2;

	if ((retval = drew_loader_new(&ldr)))
		errprint(retval, 3);
	if ((id = drew_loader_load_plugin(ldr, argv[1], NULL)) < 0)
		errprint(id, 4);
	if ((nplugins = drew_loader_get_nplugins(ldr, id)) < 0)
		errprint(nplugins, 6);

	printf("Loaded plugin %s (%d implementations).\n", argv[1], nplugins);
	for (int i = 0; i < nplugins; i++) {
		const char *name;
		drew_metadata_t md;

		if ((size = drew_loader_get_functbl(ldr, i, &tbl)) < 0)
			errprint(size, 5);
		if ((type = drew_loader_get_type(ldr, i)) < 0)
			errprint(type, 7);
		if ((retval = drew_loader_get_algo_name(ldr, i, &name)) < 0)
			errprint(retval, 8);

		printf("  Plugin %s (id %d of %d).\n", name, i, nplugins);
		printf("    Type is %#x.\n", type);
		printf("    Function table is at %p (%d bytes).\n", tbl, size);

		for (int item = 0; !drew_loader_get_metadata(ldr, i, item, &md);
				item++) {
			printf("    Metadata: %s %s (%d)\n", md.predicate, md.object,
					md.type);
		}
	}
	if (drew_loader_free(&ldr))
		return 125;

	return 0;
}