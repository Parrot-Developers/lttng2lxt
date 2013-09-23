/**
 * LTTng to GTKwave trace conversion
 *
 * Authors:
 * Ivan Djelic <ivan.djelic@parrot.com>
 * Matthieu Castet <matthieu.castet@parrot.com>
 *
 * Copyright (C) 2013 Parrot S.A.
 */

#include "lttng2lxt.h"

static struct ltt_module __modules_begin MODSECT(0);
static struct ltt_module __modules_end   MODSECT(2);
static struct ltt_module *modtab = &__modules_begin+1;
static struct hsearch_data table;

void modules_init(void)
{
	unsigned int i, modcnt;
	int status;
	ENTRY entry, *ret;

	status = hcreate_r(100, &table);
	assert(status);

	modcnt = &__modules_end-&__modules_begin-1;
	INFO("modules (%d):", modcnt);

	for (i = 0; i < modcnt; i++) {
		entry.key = (char *)modtab[i].name;
		entry.data = &modtab[i];
		status = hsearch_r(entry, ENTER, &ret, &table);
		assert(status);
		if (verbose)
			fprintf(stderr, " %s", modtab[i].name);
	}
	if (verbose)
		fprintf(stderr, "\n");
}

struct ltt_module *find_module_by_name(const char *name)
{
	ENTRY entry, *ret;

	entry.key = (char *)name;
	(void)hsearch_r(entry, FIND, &ret, &table);
	if (ret)
		return ret->data;

	/*INFO("no support for %s\n", name);*/ /*XXX*/
	return NULL;
}
