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

static int compar(const void *a, const void *b)
{
	const struct ltt_module *ma = a;
	const struct ltt_module *mb = b;
	return strcmp(ma->name, mb->name);
}

struct ltt_module *find_module_by_name(const char *name)
{
	struct ltt_module module = {.name = name}, **result;

	result = tfind(&module, &modules_root, compar);

	return result ? *result : NULL;
}

void register_module(const char *name, void (*process)(const char *modname,
						       int pass,
						       double clock,
						       int cpu,
						       void *args))
{
	struct ltt_module *module;

	module = malloc(sizeof(*module));
	if (module) {
		module->name = name;
		module->process = process;
		tsearch(module, &modules_root, compar);
	}
}

void unregister_modules(void)
{
	tdestroy(modules_root, free);
}
