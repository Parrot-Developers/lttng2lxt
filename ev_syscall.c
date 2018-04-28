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
			snprintf(p, rlen, "%s=0x%08" PRIx64, name,
				 (uint64_t)value->i64);
		else
			snprintf(p, rlen, "%s=%" PRIi64, name, value->i64);
		break;

	case ARG_U64:
		if (value->u64 >= 1000000)
			snprintf(p, rlen, "%s=0x%08" PRIx64, name, value->u64);
		else
			snprintf(p, rlen, "%s=%" PRIu64, name, value->u64);
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
		snprintf(buf, sizeof(buf), "%d: %s(%s)", cpu,
			 &modname[4], argbuf);
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

static void compat_syscall_entry_process(const char *modname, int pass, double clock, int cpu, void *args)
{
	sys_process(modname + 17, pass, clock, cpu, args);
}
MODULE_PATTERN(compat_syscall_entry, compat_syscall_entry_*);

static void syscall_entry_process(const char *modname, int pass, double clock, int cpu, void *args)
{
	sys_process(modname + 10, pass, clock, cpu, args);
}
MODULE_PATTERN(syscall_entry, syscall_entry_*);

static void exit_syscall_process(const char *modname, int pass, double clock,
				 int cpu, void *args)
{
	int ret;
	char buf[80];
	struct task *task;

	if (pass == 1)
		return;

	/* pass 2 only */

	task = get_current_task(cpu);
	if (task) {
		/*
		 * 'ret' is normally the syscall id; or the syscall return
		 * value if lttng-modules is patched accordingly
		 */
		ret = (int)get_arg_i64(args, "ret");
		snprintf(buf, sizeof(buf), "%d: ret=%d", cpu, ret);
		emit_trace(task->info_trace, (union ltt_value)buf);
		task->mode = PROCESS_USER;
		emit_trace(task->state_trace, (union ltt_value)task->mode);
	}
}
MODULE(exit_syscall);

static void compat_syscall_exit_process(const char *modname, int pass, double clock,
				 int cpu, void *args)
{
	exit_syscall_process(modname, pass, clock, cpu, args);
}

MODULE_PATTERN(compat_syscall_exit, compat_syscall_exit_*);

static void syscall_exit_process(const char *modname, int pass, double clock,
				 int cpu, void *args)
{
	exit_syscall_process(modname, pass, clock, cpu, args);
}

MODULE_PATTERN(syscall_exit, syscall_exit_*);
