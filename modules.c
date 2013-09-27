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

static void *modules_root;
static struct ltt_module *pat_modules;
static int nb_pat_modules;

static int compar(const void *a, const void *b)
{
	const struct ltt_module *ma = a;
	const struct ltt_module *mb = b;
	return strcmp(ma->name, mb->name);
}

const struct ltt_module *find_module_by_name(const char *name)
{
	int i;
	struct ltt_module module = {.name = name}, **result;

	/* first, search the tree of regular modules */
	result = tfind(&module, &modules_root, compar);
	if (result)
		return *result;

	/* not found, try pattern-matching modules */
	for (i = 0; i < nb_pat_modules; i++)
		if (fnmatch(pat_modules[i].name, name, 0) == 0)
			return &pat_modules[i];

	return NULL;
}

void register_module(const char *name, void (*process)(const char *modname,
						       int pass,
						       double clock,
						       int cpu,
						       void *args))
{
	struct ltt_module *module;

	if (strchr(name, '*') || strchr(name, '?') || strchr(name, '[')) {
		/* store pattern-matching modules in a separate array */
		pat_modules = realloc(pat_modules,
				      sizeof(*pat_modules)*(++nb_pat_modules));
		assert(pat_modules);
		pat_modules[nb_pat_modules-1].name = name;
		pat_modules[nb_pat_modules-1].process = process;
	} else {
		/* store regular modules in a binary tree */
		module = malloc(sizeof(*module));
		if (module) {
			module->name = name;
			module->process = process;
			tsearch(module, &modules_root, compar);
		}
	}
}

void unregister_modules(void)
{
	tdestroy(modules_root, free);
	free(pat_modules);
}
