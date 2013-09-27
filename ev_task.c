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
#include "lttng.h"

#define PROCESS_STATE       "proc.state.[%d-%d] %s"
#define PROCESS_INFO        "proc.sys.[%d-%d] %s (info)"

static void *root;
static struct task *current_task[MAX_CPU];

struct task *get_current_task(int cpu)
{
	return current_task[cpu];
}

static int compare(const void *pa, const void *pb)
{
	int a = ((struct task *)pa)->pid;
	int b = ((struct task *)pb)->pid;

	if (a < b)
		return -1;
	if (a > b)
		return 1;

	return 0;
}

static void update_task_tgid(int pid, int tgid)
{
	struct task task, *ret;

	task.pid = pid;
	ret = tfind(&task, &root, compare);
	if (ret) {
		ret = *((void **)ret);
		ret->tgid = tgid;
	}
}

static struct task *find_or_add_task(const char *name, int pid, int tgid)
{
	int cpu;
	struct task task, *ret;
	struct ltt_trace *data;
	char buf[32];

	task.pid = pid;
	ret = tfind(&task, &root, compare);

	if (name) {
		if (strcmp(name, "swapper") == 0) {
			name = "idle thread";
			tgid = 0;
		} else if (sscanf(name, "swapper/%d", &cpu) == 1) {
			snprintf(buf, sizeof(buf), "idle/%d thread", cpu);
			name = buf;
			tgid = 0;
		} else {
			snprintf(buf, sizeof(buf), "%s", name);
			symbol_clean_name(buf);
			name = buf;
		}
	}

	if (!ret) {
		/* this can happen if we on the first cs from this process */
		if (!name)
			name = "????";

		data = calloc(2, sizeof(struct ltt_trace));
		assert(data);

		ret = malloc(sizeof(struct task));
		assert(ret);

		ret->pid = pid;
		ret->tgid = tgid;
		ret->state_trace = &data[0];
		ret->info_trace  = &data[1];
		ret->mode = PROCESS_KERNEL;
		ret = tsearch(ret, &root, compare);
		assert(ret);
		ret = *((void **)ret);

		init_trace(ret->state_trace, TG_PROCESS, 1.0 + (tgid<<16) + pid,
			   TRACE_SYM_F_BITS, PROCESS_STATE, tgid, pid, name);

		init_trace(ret->info_trace, /*TG_PROCESS*/0,
			   1.1 + (tgid<<16) + pid,
			   TRACE_SYM_F_STRING, PROCESS_INFO, tgid, pid, name);

	} else if ((strcmp(name, "no name") != 0) &&
		   (strcmp(name, "kthreadd") != 0) /* XXX */) {

		ret = *((void **)ret);
		if ((tgid == 0) && (ret->tgid != 0))
			tgid = ret->tgid;
		ret->tgid = tgid;
		refresh_name(ret->state_trace, PROCESS_STATE, tgid, pid, name);
		refresh_name(ret->info_trace, PROCESS_INFO, tgid, pid, name);
	}
	return ret;
}

struct task *find_task(int pid)
{
	struct task *ret, task = {.pid = pid};

	ret = tfind(&task, &root, compare);
	/* XXX big hack; this should be removed we use alias to track name */
	if (ret == NULL)
		ret = find_or_add_task(NULL, pid, 0);
	else
		ret = *((void **)ret);
	assert(ret);
	return ret;
}

static
void lttng_statedump_process_state_process(const char *modname, int pass,
					   double clock, int cpu, void *args)
{
	int tid, pid, /*type,*/ mode, status;
	const char *name;
	union ltt_value value;
	struct task *task;

	tid = (int)get_arg_u64(args, "tid");
	pid = (int)get_arg_u64(args, "pid");
	name = get_arg_str(args, "name");
	/*type = (int)get_arg_u64(args, "type");*/
	mode = (int)get_arg_u64(args, "mode");
	status = (int)get_arg_u64(args, "status");

	if (pass == 1) {
		find_or_add_task(name, tid, pid);
		update_task_tgid(tid, pid);
	}

	if (pass == 2) {
		task = find_task(tid);

		switch (status) {
		case LTTNG_WAIT_FORK:
		case LTTNG_WAIT_CPU:
		case LTTNG_WAIT:
			/* skip processes which have not been running yet */
			if (!task->state_trace->emitted)
				return;
			value = (union ltt_value)PROCESS_IDLE;
			break;

		case LTTNG_RUN:
			task->mode = ((mode == LTTNG_USER_MODE) ?
				      PROCESS_USER : PROCESS_KERNEL);
			value = (union ltt_value)task->mode;
			break;

		default:
		case LTTNG_UNNAMED:
		case LTTNG_DEAD:
		case LTTNG_EXIT:
		case LTTNG_ZOMBIE:
			/* skip processes which have not been running yet */
			if (!task->state_trace->emitted)
				return;
			value = (union ltt_value)PROCESS_DEAD;
			break;
		}
		emit_trace(task->state_trace, value);
	}
}
MODULE(lttng_statedump_process_state);

static void sched_switch_process(const char *modname, int pass, double clock,
				 int cpu, void *args)
{
	int prev_tid, next_tid, prev_state;
	const char *prev_comm, *next_comm;
	struct task *task;
/*
 * prev_state:
  { 1, "S"} , { 2, "D" }, { 4, "T" }, { 8, "t" }, { 16, "Z" }, { 32, "X" },
  { 64, "x" }, { 128, "W" }) : "R",
*/
	prev_comm = get_arg_str(args, "prev_comm");
	prev_tid = (int)get_arg_u64(args, "prev_tid");
	prev_state = (int)get_arg_u64(args, "prev_state");
	next_comm = get_arg_str(args, "next_comm");
	next_tid = (int)get_arg_u64(args, "next_tid");

	if (pass == 1) {
		find_or_add_task(prev_comm, prev_tid, 0/*XXX*/);
		find_or_add_task(next_comm, next_tid, 0/*XXX*/);
	}

	if (pass == 2) {
		task = find_task(prev_tid);

		/* restore idle state only if prev process is not dead */
		if (!(prev_state & 0x70 /*XXX*/))
			emit_trace(task->state_trace,
				   (union ltt_value)PROCESS_IDLE);
		else
			emit_trace(task->state_trace,
				   (union ltt_value)PROCESS_DEAD);

		if (prev_tid == 0)
			set_cpu_idle(clock, cpu);

		/* emit state of newly scheduled task */
		task = find_task(next_tid);
		emit_trace(task->state_trace, (union ltt_value)task->mode);
		current_task[cpu] = task;

		if (next_tid == 0)
			set_cpu_running(clock, cpu);
	}
}
MODULE(sched_switch);

static void sched_wakeup_process(const char *modname, int pass, double clock,
				 int cpu, void *args)
{
	int tid;
	const char *comm;
	struct task *task;

	comm = get_arg_str(args, "comm");
	tid = (int)get_arg_u64(args, "tid");

	if (pass == 1)
		find_or_add_task(comm, tid, 0/*XXX*/);

	if (pass == 2) {
		task = find_task(tid);
		emit_trace(task->state_trace, (union ltt_value)PROCESS_WAKEUP);
	}
}
MODULE(sched_wakeup);

static void sched_wakeup_new_process(const char *modname, int pass,
				     double clock, int cpu, void *args)
{
	sched_wakeup_process(modname, pass, clock, cpu, args);
}
MODULE(sched_wakeup_new);

static void sched_process_wait_process(const char *modname, int pass,
				       double clock, int cpu, void *args)
{
	int tid;
	const char *comm;
	struct task *task;

	comm = get_arg_str(args, "comm");
	tid = (int)get_arg_u64(args, "tid");

	/*FIXME*/
	if (!tid)
		return;

	if (pass == 1)
		find_or_add_task(comm, tid, 0);

	if (pass == 2) {
		task = find_task(tid);
		emit_trace(task->state_trace, (union ltt_value)PROCESS_IDLE);
	}
}
MODULE(sched_process_wait);

/*
 * 'sched_process_exit' happens way before the process really exits, track
 * state using 'sched_process_free' instead.
 */
static void sched_process_free_process(const char *modname, int pass,
				       double clock, int cpu, void *args)
{
	int tid;
	const char *comm;
	struct task *task;

	comm = get_arg_str(args, "comm");
	tid = (int)get_arg_u64(args, "tid");

	if (pass == 1)
		find_or_add_task(comm, tid, 0);

	if (pass == 2) {
		task = find_task(tid);
		emit_trace(task->state_trace, (union ltt_value)PROCESS_DEAD);
	}
}
MODULE(sched_process_free);

static void sched_process_fork_process(const char *modname, int pass,
				       double clock, int cpu, void *args)
{
/*
	int child_tid;
	const char *child_comm;

	child_comm = get_arg_str(args, "child_comm");
	child_tid = (int)get_arg_u64(args, "child_tid");

	if (pass == 1)
		find_or_add_task(child_comm, child_tid, child_tid);
*/
}
MODULE(sched_process_fork);
