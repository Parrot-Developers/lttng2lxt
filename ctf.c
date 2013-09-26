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

#include <babeltrace/babeltrace.h>
#include <babeltrace/ctf/iterator.h>
#include <babeltrace/ctf/events.h>
#include <babeltrace/ctf/callbacks.h>
#include <ftw.h>
#include <fcntl.h>

static struct bt_context *ctx;
static uint32_t tids;

void get_arg(void *args, const char *name, struct arg_value *value)
{
	struct bt_ctf_event *ctf_event = args;
	const struct bt_definition *scope;
	const struct bt_definition *def;
	const struct bt_declaration *decl;
	enum ctf_type_id type;

	scope = bt_ctf_get_top_level_scope(ctf_event, BT_EVENT_FIELDS);
	def = bt_ctf_get_field(ctf_event, scope, name);
	assert(def);
	decl = bt_ctf_get_decl_from_def(def);
	assert(decl);
	type = bt_ctf_field_type(decl);

	switch (type) {

	case CTF_TYPE_INTEGER:
		if (bt_ctf_get_int_signedness(decl)) {
			value->i64 = bt_ctf_get_int64(def);
			value->type = ARG_I64;
		} else {
			value->u64 = bt_ctf_get_uint64(def);
			value->type = ARG_U64;
		}
		break;

	case CTF_TYPE_STRING:
		value->s = bt_ctf_get_string(def);
		assert(value->s);
		value->type = ARG_STR;
		break;

	case CTF_TYPE_ARRAY:
		value->s = bt_ctf_get_char_array(def);
		assert(value->s);
		value->type = ARG_STR;
		break;

	case CTF_TYPE_STRUCT:
	case CTF_TYPE_UNTAGGED_VARIANT:
	case CTF_TYPE_VARIANT:
	case CTF_TYPE_FLOAT:
	case CTF_TYPE_ENUM:
	case CTF_TYPE_SEQUENCE:
	default:
		FATAL("unsupported CTF type\n");
		break;
	}
}

void for_each_arg(void *args,
		  void (*pfn)(void *cookie,
			      const char *name,
			      const struct arg_value *value),
		  void *cookie)
{
	int ret;
	struct bt_ctf_event *ctf_event = args;
	unsigned int count, i;
	struct bt_definition const * const *list;
	struct arg_value value;
	const char *name;
	const struct bt_definition *scope;

	scope = bt_ctf_get_top_level_scope(ctf_event, BT_STREAM_PACKET_CONTEXT);
	assert(scope);

	ret = bt_ctf_get_field_list(ctf_event, scope, &list, &count);
	assert(ret == 0);

	for (i = 0; i < count; i++) {
		name = bt_ctf_field_name(list[i]);
		get_arg(args, name, &value);
		(*pfn)(cookie, name, &value);
	}
}

int64_t get_arg_i64(void *args, const char *name)
{
	struct arg_value value;
	get_arg(args, name, &value);
	return value.i64;
}

uint64_t get_arg_u64(void *args, const char *name)
{
	struct arg_value value;
	get_arg(args, name, &value);
	return value.u64;
}

const char *get_arg_str(void *args, const char *name)
{
	struct arg_value value;
	get_arg(args, name, &value);
	return value.s;
}

static void process_one_event(struct bt_ctf_event *ctf_event, double clock,
			      struct ltt_module *mod, int pass)
{
	int i;
	const struct bt_definition *scope;
	const struct bt_definition *def;
	int cpu_id;

	scope = bt_ctf_get_top_level_scope(ctf_event, BT_STREAM_PACKET_CONTEXT);
	assert(scope);
	def = bt_ctf_get_field(ctf_event, scope, "cpu_id");
	assert(def);

	cpu_id = (int)bt_ctf_get_uint64(def);
	if (cpu_id >= MAX_CPU) {
		DIAG("dropping event with cpu_id = %d\n", cpu_id);
		return;
	}

	if (pass == 2)
		emit_clock(clock);

	mod->process(mod->name, pass, clock, cpu_id, ctf_event);
}

static void process_events(struct bt_ctf_iter *iter, int pass)
{
	int ret;
	double clock;
	const char *name;
	struct bt_ctf_event *ctf_event;
	struct ltt_module *mod;

	while ((ctf_event = bt_ctf_iter_read_event(iter))) {
		name = bt_ctf_event_name(ctf_event);
		mod = find_module_by_name(name);
		if (mod) {
			clock = (double)bt_ctf_get_timestamp(ctf_event)/
				1000000000.0;
			process_one_event(ctf_event, clock, mod, pass);
		}
		ret = bt_iter_next(bt_ctf_get_iter(iter));
		if (ret < 0)
			FATAL("error fetching event in trace '%s'\n", name);
	}
}

static int traverse_trace_dir(const char *fpath, const struct stat *sb,
			      int tflag, struct FTW *ftwbuf)
{
	int tid;
	/* size of "fpath/metadata" + '0' */
	size_t sz = sizeof(char) * (strlen(fpath) + strlen("metadata") + 2);

	char *metadata = malloc(sz);
	if (!metadata)
		return -1;

	snprintf(metadata, sz, "%s/%s", fpath, "metadata");
	if (access(metadata, R_OK))
		goto exit;

	tid = bt_context_add_trace(ctx, fpath, "ctf",
			NULL, NULL, NULL);
	if (tid < 0)
		FATAL("cannot open trace '%s' for reading\n", fpath);

	if (tid > 31)
		goto exit;

	tids |= 1 << tid;

exit:
	free(metadata);
	return 0;
}

void scan_lttng_trace(const char *name)
{
	struct bt_ctf_iter *iter;
	struct bt_iter_pos begin_pos;
	int ret, i;

	ctx = bt_context_create();
	assert(ctx);

	ret = nftw(name, traverse_trace_dir, 10, 0);
	if (ret < 0)
		FATAL("cannot open trace '%s'\n", name);

	begin_pos.type = BT_SEEK_BEGIN;
	iter = bt_ctf_iter_create(ctx, &begin_pos, NULL);
	if (!iter)
		FATAL("cannot iterate on trace '%s'\n", name);

	INFO("pass 1: initializing modules and converting addresses\n");
	process_events(iter, 1);

	/* flush address symbol conversion pipe */
	atag_flush();
	symbol_flush();

	INFO("pass 2: emitting LXT traces\n");
	/* rewind */
	ret = bt_iter_set_pos(bt_ctf_get_iter(iter), &begin_pos);
	assert(ret == 0);
	process_events(iter, 2);

	bt_ctf_iter_destroy(iter);
	i = 0;
	while (tids) {
		if (tids & (1 << i)) {
			bt_context_remove_trace(ctx, i);
			tids &= ~(1 << i);
			i++;
		}
	}

	bt_context_put(ctx);
}

