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

#define MAX_ARGS_LEN  (80)

static void dump_syscall_arg(void *cookie, const char *name,
			     const struct arg_value *value)
{
	int len, rlen;
	char *argbuf = cookie, *p;

	len = strlen(argbuf);
	p = &argbuf[len];
	rlen = MAX_ARGS_LEN-len;

	if (rlen <= 3)
		return;

	if (len > 0) {
		/* this is not the first argument, prefix with a comma */
		*p++ = ',';
		*p++ = ' ';
		rlen -= 2;
	}

	switch (value->type) {
	case ARG_I64:
		if (value->i64 >= 1000000 || value->i64 <= -1000000)
			snprintf(p, rlen, "%s=0x%08llx", name,
				 (uint64_t)value->i64);
		else
			snprintf(p, rlen, "%s=%lld", name, value->i64);
		break;

	case ARG_U64:
		if (value->u64 >= 1000000)
			snprintf(p, rlen, "%s=0x%08llx", name, value->u64);
		else
			snprintf(p, rlen, "%s=%llu", name, value->u64);
		break;

	case ARG_STR:
	default:
		snprintf(p, rlen, "%s=\"%s\"", name, value->s);
		break;
	}
}

static void sys_process(const char *modname, int pass, double clock, int cpu,
			void *args)
{
	char argbuf[MAX_ARGS_LEN] = "";
	char buf[80];
	struct task *task;

	if (pass == 2) {
		/* dump syscall arguments */
		for_each_arg(args, dump_syscall_arg, argbuf);
		snprintf(buf, sizeof(buf), "%s(%s)", &modname[4], argbuf);
		task = get_current_task(cpu);
		if (task) {
			emit_trace(task->info_trace, (union ltt_value)buf);
			task->mode = PROCESS_KERNEL;
			emit_trace(task->state_trace,
				   (union ltt_value)task->mode);
		}
	}
}
MODULE_PATTERN(sys, sys_*);

static void exit_syscall_process(const char *modname, int pass, double clock,
				 int cpu, void *args)
{
	struct task *task;

	if (pass == 1)
		return;

	/* pass 2 only */

	/*
	 * 'ret' is the syscall id, there is no much point showing it
	 * ret = (int)get_arg_i64(args, "ret");
	 */

	task = get_current_task(cpu);
	if (task) {
		emit_trace(task->info_trace, (union ltt_value)"");
		task->mode = PROCESS_USER;
		emit_trace(task->state_trace, (union ltt_value)task->mode);
	}
}
MODULE(exit_syscall);
