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
	HI_SOFTIRQ = 0,
	TIMER_SOFTIRQ,
	NET_TX_SOFTIRQ,
	NET_RX_SOFTIRQ,
	BLOCK_SOFTIRQ,
	TASKLET_SOFTIRQ,
	SCHED_SOFTIRQ,
/*#ifdef CONFIG_HIGH_RES_TIMERS*/
	HRTIMER_SOFTIRQ,
/*#endif*/
	RCU_SOFTIRQ,    /* Preferable RCU should always be the last softirq */
};

#define str(x) [x] = #x

static char *sofirq_tag[] = {
	str(HI_SOFTIRQ),
	str(TIMER_SOFTIRQ),
	str(NET_TX_SOFTIRQ),
	str(NET_RX_SOFTIRQ),
	str(BLOCK_SOFTIRQ),
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

static int softirqstate[MAX_CPU];
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

	if (irqlevel[cpu] > 0) {
		emit_trace(&trace[cpu][irqtab[cpu][irqlevel[cpu]-1]],
			   (union ltt_value)IRQ_RUNNING);
	}
	cpu_unpreempt(clock, cpu);
}
MODULE(irq_handler_exit);

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
}
MODULE(softirq_exit);
