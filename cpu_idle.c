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

#define IDLE_CPU_IDLE    LT_IDLE
#define IDLE_CPU_RUNNING LT_S0
#define IDLE_CPU_PREEMPT LT_IDLE

enum {
	IDLE_IDLE,
	IDLE_RUNNING,
};

static int idle_cpu_state[MAX_CPU];
static int idle_cpu_preempt[MAX_CPU];
static struct ltt_trace idle_cpu[MAX_CPU];

static double emit_cpu_idle_state(double clock, int cpu, union ltt_value val)
{
	static double run_start;
	static double total_run;
	double ret;

	init_trace(&idle_cpu[cpu], TG_PROCESS, 0.0+0.001*cpu, TRACE_SYM_F_BITS,
		   "cpu idle/%d", cpu);

	if (val.state) {
		emit_trace(&idle_cpu[cpu], val);

		/* if running account the time */
		if (strcmp(val.state, IDLE_CPU_RUNNING) == 0) {
			/* if already started, do nothing */
			if (!run_start)
				run_start = clock;
		} else { /* if stopping, record the idle time */
			/* if already stopped,, do nothing */
			if (run_start > 0) {
				total_run += clock - run_start;
				run_start = 0;
			}
		}
		ret = total_run;
	} else { /* give the total idle time and reset counter */
		/* idle is running */
		if (run_start > 0) {
			total_run += clock - run_start;
			run_start = clock;
		}
		ret = total_run;
		/* reset counter */
		total_run = 0;
	}
	return ret;
}

void set_cpu_idle(double clock, int cpu)
{
	idle_cpu_state[cpu] = IDLE_IDLE;
	(void)emit_cpu_idle_state(clock, cpu, (union ltt_value)IDLE_CPU_IDLE);
}

void set_cpu_running(double clock, int cpu)
{
	idle_cpu_state[cpu] = IDLE_RUNNING;
	(void)emit_cpu_idle_state(clock, cpu,
				  (union ltt_value)IDLE_CPU_RUNNING);
}

void cpu_preempt(double clock, int cpu)
{
	idle_cpu_preempt[cpu]++;
	if (idle_cpu_preempt[cpu] == 1)
		(void)emit_cpu_idle_state(clock, cpu,
					  (union ltt_value)IDLE_CPU_PREEMPT);
}

void cpu_unpreempt(double clock, int cpu)
{
	union ltt_value value;

	if (idle_cpu_preempt[cpu] <= 0)
		return;

	idle_cpu_preempt[cpu]--;

	if (idle_cpu_preempt[cpu] == 0) {
		if (idle_cpu_state[cpu] == IDLE_RUNNING)
			value.state = IDLE_CPU_RUNNING;
		else
			value.state = IDLE_CPU_IDLE;
		(void)emit_cpu_idle_state(clock, cpu, value);
	}
}
