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
#define PROCESS_INFO        "proc.info.[%d-%d] %s (info)"

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

static void update_task(struct task *task, const char *name, int pid, int tgid)
{
	int need_refresh = 0;

	if (name && strcmp(name, task->name)) {
		free(task->name);
		task->name = strdup(name);
		need_refresh = 1;
	}
	if (pid && (pid != task->pid)) {
		task->pid = pid;
		need_refresh = 1;
	}

	if (tgid && (tgid != task->tgid)) {
		task->tgid = tgid;
		need_refresh = 1;
	}

	/* refresh trace name if necessary */
	if (need_refresh) {
		refresh_name(task->state_trace, PROCESS_STATE, task->tgid,
			     task->pid, task->name);
		refresh_name(task->info_trace, PROCESS_INFO, task->tgid,
			     task->pid, task->name);
	}
}

static struct task *new_task(const char *name, int pid)
{
	struct task *task;
	struct ltt_trace *data;

	/* this can happen if we on the first cs from this process */
	if (!name)
		name = "????";

	data = calloc(2, sizeof(struct ltt_trace));
	assert(data);

	task = malloc(sizeof(struct task));
	assert(task);

	task->name = strdup(name);
	task->pid = pid;
	/* tgid will be updated later */
	task->tgid = 0;
	task->state_trace = &data[0];
	task->info_trace  = &data[1];
	task->mode = PROCESS_KERNEL;

	init_trace(task->state_trace, TG_PROCESS,
		   1.0 + (task->tgid << 16) + task->pid,
		   TRACE_SYM_F_BITS, PROCESS_STATE, task->tgid, task->pid,
		   task->name);

	init_trace(task->info_trace, /*TG_PROCESS*/0,
		   1.1 + (task->tgid << 16) + task->pid,
		   TRACE_SYM_F_STRING, PROCESS_INFO, task->tgid,
		   task->pid, task->name);

	/* add new task to tree */
	task = tsearch(task, &root, compare);
	assert(task);
	task = *((void **)task);

	return task;
}

struct task *find_or_add_task(const char *name, int pid)
{
	int cpu;
	struct task task, *ret;
	char buf[32];

	task.pid = pid;
	ret = tfind(&task, &root, compare);

	if (name) {
		/* show 'idle thread' instead of 'swapper' */
		if (strcmp(name, "swapper") == 0) {
			name = "idle thread";
		} else if (sscanf(name, "swapper/%d", &cpu) == 1) {
			snprintf(buf, sizeof(buf), "idle/%d thread", cpu);
			name = buf;
		} else {
			snprintf(buf, sizeof(buf), "%s", name);
			symbol_clean_name(buf);
			name = buf;
		}
	}

	if (!ret) {
		ret = new_task(name, pid);
	} else {
		ret = *((void **)ret);
		if (name)
			/* just refresh task name */
			update_task(ret, name, 0, 0);
	}
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

	if (pass == 1) {
		name = get_arg_str(args, "name");
		pid = (int)get_arg_u64(args, "pid");
		task = find_or_add_task(name, tid);
		/* set tgid */
		update_task(task, NULL, 0, pid);
	}

	if (pass == 2) {
		/*type = (int)get_arg_u64(args, "type");*/
		mode = (int)get_arg_u64(args, "mode");
		status = (int)get_arg_u64(args, "status");
		task = find_or_add_task(NULL, tid);

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
		find_or_add_task(prev_comm, prev_tid);
		find_or_add_task(next_comm, next_tid);
	}

	if (pass == 2) {
		task = find_or_add_task(NULL, prev_tid);

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
		task = find_or_add_task(NULL, next_tid);
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

	tid = (int)get_arg_u64(args, "tid");

	if (pass == 1) {
		comm = get_arg_str(args, "comm");
		find_or_add_task(comm, tid);
	}

	if (pass == 2) {
		task = find_or_add_task(NULL, tid);
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

	tid = (int)get_arg_u64(args, "tid");

	/* why is tid == 0 on some sched_process_wait events ? */
	if (!tid)
		return;

	if (pass == 1) {
		comm = get_arg_str(args, "comm");
		find_or_add_task(comm, tid);
	}

	if (pass == 2) {
		task = find_or_add_task(NULL, tid);
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

	tid = (int)get_arg_u64(args, "tid");

	if (pass == 1) {
		comm = get_arg_str(args, "comm");
		find_or_add_task(comm, tid);
	}

	if (pass == 2) {
		task = find_or_add_task(NULL, tid);
		emit_trace(task->state_trace, (union ltt_value)PROCESS_DEAD);
	}
}
MODULE(sched_process_free);

static struct ltt_trace sched_fork[MAX_CPU];

static void sched_process_fork_process(const char *modname, int pass,
				       double clock, int cpu, void *args)
{
	const char *parent_comm;
	int parent_tid, child_tid;

	if (pass == 1)
		init_trace(&sched_fork[cpu], TG_GLOBAL, 1.0+0.1*cpu,
			   TRACE_SYM_F_STRING, "fork/%d", cpu);

	if (pass == 2) {
		parent_comm = get_arg_str(args, "parent_comm");
		parent_tid = (int)get_arg_u64(args, "parent_tid");
		child_tid = (int)get_arg_u64(args, "child_tid");

		emit_trace(&sched_fork[cpu], (union ltt_value)"[%d] %s -> [%d]",
			   parent_tid, parent_comm, child_tid);
	}
}
MODULE(sched_process_fork);

static void sched_process_exec_process(const char *modname, int pass,
				       double clock, int cpu, void *args)
{
	int pid;
	struct task *task;

	if (pass == 1) {
		pid = (int)get_arg_u64(args, "pid");
		task = find_or_add_task(NULL, pid);
		/* after exec we know this task's tgid */
		update_task(task, NULL, 0, pid);
	}
}
MODULE(sched_process_exec);

static void sched_migrate_task_process(const char *modname, int pass,
				       double clock, int cpu, void *args)
{
	const char *comm;
	struct task *task;
	int orig_cpu, dest_cpu, tid;

	tid = (int)get_arg_u64(args, "tid");

	if (pass == 1) {
		comm = get_arg_str(args, "comm");
		(void)find_or_add_task(comm, tid);
	}

	if (pass == 2) {
		orig_cpu = (int)get_arg_u64(args, "orig_cpu");
		dest_cpu = (int)get_arg_u64(args, "dest_cpu");

		task = find_or_add_task(NULL, tid);
		if (task) {
			emit_trace(task->info_trace,
				   (union ltt_value)"cpu%d->cpu%d",
				   orig_cpu, dest_cpu);
		}
	}
}
MODULE(sched_migrate_task);

static void sched_stat_runtime_process(const char *modname, int pass,
				       double clock, int cpu, void *args)
{
	const char *comm;
	int tid;

	if (pass == 1) {
		comm = get_arg_str(args, "comm");
		tid = (int)get_arg_u64(args, "tid");
		(void)find_or_add_task(comm, tid);
	}
}
MODULE(sched_stat_runtime);
