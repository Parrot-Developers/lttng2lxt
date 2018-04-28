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

#define IRQ_IDLE         LT_IDLE
#define IRQ_RUNNING      LT_S0
#define IRQ_PREEMPT      (gtkwave_parrot ? LT_S2 : LT_0)

#define SOFTIRQ_IDLE     LT_IDLE
#define SOFTIRQ_RUNNING  LT_S0
#define SOFTIRQ_RAISING  (gtkwave_parrot ? LT_S2 : LT_0)

/* nested softirq state */
enum {
	SOFTIRQS_IDLE,
	SOFTIRQS_RAISE,
	SOFTIRQS_RUN,
};

/* sofirq val */
enum {
	HI_SOFTIRQ=0,
	TIMER_SOFTIRQ,
	NET_TX_SOFTIRQ,
	NET_RX_SOFTIRQ,
	BLOCK_SOFTIRQ,
	BLOCK_IOPOLL_SOFTIRQ,
	TASKLET_SOFTIRQ,
	SCHED_SOFTIRQ,
	HRTIMER_SOFTIRQ, /* Unused, but kept as tools rely on the
			    numbering. Sigh! */
	RCU_SOFTIRQ,    /* Preferable RCU should always be the last softirq */

	NR_SOFTIRQS
};

#define str(x) [x] = #x

static char *sofirq_tag[] = {
	str(HI_SOFTIRQ),
	str(TIMER_SOFTIRQ),
	str(NET_TX_SOFTIRQ),
	str(NET_RX_SOFTIRQ),
	str(BLOCK_SOFTIRQ),
	str(BLOCK_IOPOLL_SOFTIRQ),
	str(TASKLET_SOFTIRQ),
	str(SCHED_SOFTIRQ),
	str(HRTIMER_SOFTIRQ),
	str(RCU_SOFTIRQ),
};
#undef str

/* nested irq stack */
static int irqtab[MAX_CPU][MAX_IRQS];
static int irqlevel[MAX_CPU];
static char *irq_tag[MAX_IRQS];
static struct {
	double entry_time;
	double max_delta_us;
	double max_delta_position;
	double total_time;
	unsigned int counter;
} irqstat[MAX_CPU][MAX_IRQS] = {
	[0 ... MAX_CPU-1][ 0 ... MAX_IRQS-1] = { 0.0, 0.0, 0.0, 0.0, 0}
};

static int softirqstate[MAX_CPU];
static struct {
	double entry_time;
	double max_delta_us;
	double max_delta_position;
	double total_time;
	unsigned int counter;
} softirqstat[MAX_CPU] = {
	[0 ... MAX_CPU-1] = { 0.0, 0.0, 0.0, 0.0, 0}
};

/*
static double softirqtime;
static char softirqtask[30];
static double irqtime;
*/

static struct ltt_trace trace[MAX_CPU][MAX_IRQS];
static struct ltt_trace sirq[MAX_CPU][3];

static void update_irq_name(int irq, const char *name)
{
	char buf[40];

	snprintf(buf, sizeof(buf), "%s (%d)", name, irq);
	symbol_clean_name(buf);

	if (irq < MAX_IRQS) {
		if (!irq_tag[irq] || strcmp(irq_tag[irq], buf)) {
			INFO("%s -> %s\n", irq_tag[irq] ? : "<null>", buf);
			free(irq_tag[irq]);
			irq_tag[irq] = strdup(buf);
		}
	}
}

static void irq_handler_entry_process(const char *modname, int pass,
				      double clock, int cpu, void *args)
{
	int irq;
	const char *name;

	irq  = (int)get_arg_i64(args, "irq");
	name = get_arg_str(args, "name");

	/*
	  INFO("irq_handler_entry: irq %d name '%s'\n", irq, name);
	*/

	if (irq >= MAX_IRQS) {
		DIAG("invalid IRQ vector ? (%d)\n", irq);
		return;
	}

	if (pass == 1) {
		update_irq_name(irq, name);
		init_trace(&trace[cpu][irq], TG_IRQ, 1.0+irq, TRACE_SYM_F_BITS,
			   "%s/%d", irq_tag[irq], cpu);
		/*atag_store(ip);*/
		init_cpu(cpu);
	}

	if (pass == 2) {
		if (irqlevel[cpu] >= MAX_IRQS) {
			DIAG("IRQ nesting level is too high (%d)\n",
			     irqlevel[cpu]);
			return;
		}
		if ((irqlevel[cpu] > 0) &&
		    (irq == irqtab[cpu][irqlevel[cpu]-1])) {
			DIAG("IRQ reentering in same irq (broken trace ?) %d\n",
			     irqlevel[cpu]);
			return;
		}

		if (irqlevel[cpu] > 0) {
			TDIAG("irq_handler", clock, "nesting irq %s -> %s\n",
			      irq_tag[irqtab[cpu][irqlevel[cpu]-1]],
			      irq_tag[irq]);
			emit_trace(&trace[cpu][irqtab[cpu][irqlevel[cpu]-1]],
				   (union ltt_value)IRQ_PREEMPT);
		}
		emit_trace(&trace[cpu][irq], (union ltt_value)IRQ_RUNNING);
		/*emit_trace(&irq_pc, (union ltt_value)ip);*/
		irqtab[cpu][irqlevel[cpu]++] = irq;
		if (do_stats & STAT_IRQ)
			irqstat[cpu][irq].entry_time = clock;

		cpu_preempt(clock, cpu);
	}
}
MODULE(irq_handler_entry);

static void irq_handler_exit_process(const char *modname, int pass,
				     double clock, int cpu, void *args)
{
	if ((pass == 1) || (irqlevel[cpu] <= 0))
		return;

	emit_trace(&trace[cpu][irqtab[cpu][--irqlevel[cpu]]],
		   (union ltt_value)IRQ_IDLE);

	if (do_stats & STAT_IRQ) {
		int irq = irqtab[cpu][irqlevel[cpu]];
		double delta = clock - irqstat[cpu][irq].entry_time;
		if (delta > irqstat[cpu][irq].max_delta_us) {
			irqstat[cpu][irq].max_delta_us = delta;
			irqstat[cpu][irq].max_delta_position = irqstat[cpu][irq].entry_time;
		}
		irqstat[cpu][irq].total_time += delta;
		irqstat[cpu][irq].counter++;
	}

	if (irqlevel[cpu] > 0) {
		emit_trace(&trace[cpu][irqtab[cpu][irqlevel[cpu]-1]],
			   (union ltt_value)IRQ_RUNNING);
	}
	cpu_unpreempt(clock, cpu);
}
MODULE(irq_handler_exit);

void irq_stats(void)
{
	int i;
	int cpu;
	double max = 0.0;
	if (!(do_stats & STAT_IRQ))
		return;

	DIAG("irq stat\n");
	for (cpu = 0; cpu < MAX_CPU; cpu++) {
		for (i = 0; i < MAX_IRQS; i++) {
			if (irqstat[cpu][i].max_delta_position != 0.0) {
				DIAG("irq %3d/%d count %d (%f s) max time %f us @%f s for %s/%d\n",
						i, cpu, irqstat[cpu][i].counter,
						irqstat[cpu][i].total_time,
						irqstat[cpu][i].max_delta_us,
						irqstat[cpu][i].max_delta_position,
						irq_tag[i], cpu);
				if (irqstat[cpu][i].max_delta_us > max)
					max = irqstat[cpu][i].max_delta_us;
			}
		}
	}
	DIAG(" max %f\n", max);
}

static void init_traces_softirq(int cpu)
{
	init_trace(&sirq[cpu][0], TG_IRQ, 100.0+0.02*cpu, TRACE_SYM_F_BITS,
		   "softirq/%d", cpu);
	init_trace(&sirq[cpu][1], TG_IRQ, 100.01+0.02*cpu, TRACE_SYM_F_STRING,
		   "softirq/%d (info)", cpu);
}

static void softirq_entry_process(const char *modname, int pass, double clock,
				  int cpu, void *args)
{
	int vec;

	vec = (int)get_arg_i64(args, "vec");

	if (pass == 1) {
		init_traces_softirq(cpu);
		return;
	}

	/* pass 2 */
	emit_trace(&sirq[cpu][0], (union ltt_value)SOFTIRQ_RUNNING);
	cpu_preempt(clock, cpu);

	if ((vec < ARRAY_SIZE(sofirq_tag)) && sofirq_tag[vec])
		emit_trace(&sirq[cpu][1], (union ltt_value)sofirq_tag[vec]);
	else
		emit_trace(&sirq[cpu][1], (union ltt_value)"softirq %d", vec);

	softirqstate[cpu] = SOFTIRQS_RUN;

	if (do_stats & STAT_SOFTIRQ)
		softirqstat[cpu].entry_time = clock;
}
MODULE(softirq_entry);

static void softirq_exit_process(const char *modname, int pass, double clock,
				 int cpu, void *args)
{
	if (pass == 1) {
		init_traces_softirq(cpu);
		return;
	}

	/* pass 2 */
	if (softirqstate[cpu] == SOFTIRQS_RAISE)
		emit_trace(&sirq[cpu][0], (union ltt_value)SOFTIRQ_RAISING);
	else
		emit_trace(&sirq[cpu][0], (union ltt_value)SOFTIRQ_IDLE);

	cpu_unpreempt(clock, cpu);
	softirqstate[cpu] = SOFTIRQS_IDLE;

	if (do_stats & STAT_SOFTIRQ) {
		double delta = clock - softirqstat[cpu].entry_time;
		if (delta > softirqstat[cpu].max_delta_us) {
			softirqstat[cpu].max_delta_us = delta;
			softirqstat[cpu].max_delta_position = softirqstat[cpu].entry_time;
		}
		softirqstat[cpu].total_time += delta;
		softirqstat[cpu].counter++;
	}
}
MODULE(softirq_exit);

static void irq_softirq_entry_process(const char *modname, int pass,
					double clock, int cpu, void *args)
{
	softirq_entry_process(modname, pass, clock, cpu, args);
}
MODULE(irq_softirq_entry);

static void irq_softirq_exit_process(const char *modname, int pass,
					double clock, int cpu, void *args)
{
	softirq_exit_process(modname, pass, clock, cpu, args);
}
MODULE(irq_softirq_exit);

static void irq_softirq_raise_process(const char *modname, int pass,
					double clock, int cpu, void *args)
{
	if (pass == 1) {
		init_traces_softirq(cpu);
		return;
	}

	if (softirqstate[cpu] == SOFTIRQS_IDLE)
		emit_trace(&sirq[cpu][0], (union ltt_value)SOFTIRQ_RAISING);

	softirqstate[cpu] = SOFTIRQS_RAISE;
}
MODULE(irq_softirq_raise);

void softirq_stats(void)
{
	int cpu;
	if (!(do_stats & STAT_SOFTIRQ))
		return;

	DIAG("softirq stat\n");
	for (cpu = 0; cpu < MAX_CPU; cpu++) {
		if (softirqstat[cpu].max_delta_position != 0.0) {
			DIAG("softirq/%d count %d (%f s) max time %f us @%f s\n",
					cpu, softirqstat[cpu].counter,
					softirqstat[cpu].total_time,
					softirqstat[cpu].max_delta_us,
					softirqstat[cpu].max_delta_position);
		}
	}
}
