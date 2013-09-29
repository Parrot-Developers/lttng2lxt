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

/* This is not valid for alpha, sparc and mips */
static const char * const signal_name[] = {
	[1]  = "HUP",
	[2]  = "INT",
	[3]  = "QUIT",
	[4]  = "ILL",
	[5]  = "TRAP",
	[6]  = "ABRT",
	[7]  = "BUS",
	[8]  = "FPE",
	[9]  = "KILL",
	[10] = "USR1",
	[11] = "SEGV",
	[12] = "USR2",
	[13] = "PIPE",
	[14] = "ALRM",
	[15] = "TERM",
	[17] = "CHLD",
	[18] = "CONT",
	[19] = "STOP",
	[20] = "TSTP",
};

static void emit_signal(struct task *task, int sig)
{
	const char *name = "NAL";

	if ((sig < (int)ARRAY_SIZE(signal_name)) && (signal_name[sig]))
		name = signal_name[sig];
	emit_trace(task->info_trace, (union ltt_value)"SIG%s(%d)", name, sig);
}

static void signal_generate_process(const char *modname, int pass,
				    double clock, int cpu, void *args)
{
	int sig, pid;
	const char *comm;
	struct task *task;

	comm = get_arg_str(args, "comm");
	pid = (int)get_arg_u64(args, "pid");

	if (pass == 1)
		(void)find_or_add_task(comm, pid);

	if (pass == 2) {
		sig = (int)get_arg_u64(args, "sig");
		task = find_or_add_task(NULL, pid);
		if (task)
			emit_signal(task, sig);
	}
}
MODULE(signal_generate);

static void signal_deliver_process(const char *modname, int pass,
				    double clock, int cpu, void *args)
{
	int sig;
	struct task *task;

	if (pass == 2) {
		sig = (int)get_arg_u64(args, "sig");
		task = get_current_task(cpu);
		if (task)
			emit_signal(task, sig);
	}
}
MODULE(signal_deliver);
