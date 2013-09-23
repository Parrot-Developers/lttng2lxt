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
#include "savefile.h"

static unsigned int count_traces(void)
{
	unsigned int nb = 0;
	struct ltt_trace *trace;

	for (trace = trace_head(); trace; trace = trace->next)
		nb++;

	return nb;
}

static int compare_traces(const void *t1, const void *t2)
{
	double d = (*(struct ltt_trace **)t1)->pos-
		(*(struct ltt_trace **)t2)->pos;
	return (d > 0.0) ? 1 : ((d < 0.0) ? -1 : 0);
}

static void sort_traces(enum trace_group group, struct ltt_trace **tab,
			int *len)
{
	int n;
	struct ltt_trace *trace;

	/* filter traces */
	for (trace = trace_head(), n = 0; trace; trace = trace->next) {
		if ((trace->group == group) && trace->emitted)
			tab[n++] = trace;
	}
	/* sort traces */
	if (n > 0)
		qsort(tab, n, sizeof(struct ltt_trace *), &compare_traces);

	*len = n;
}

static void print_group(enum trace_group group, const char *name, FILE *fp,
			struct ltt_trace **tab)
{
	int i, tablen;
	unsigned int flag;

	sort_traces(group, tab, &tablen);
	if (tablen <= 0)
		return;

	fprintf(fp, "@%x\n-%s\n", TR_BLANK, name);

	for (i = 0; i < tablen; i++) {
		flag = 0;
		flag |= (tab[i]->flags & TRACE_SYM_F_BITS) ?    TR_BIN : 0;
		flag |= (tab[i]->flags & TRACE_SYM_F_INTEGER) ? TR_HEX : 0;
		flag |= (tab[i]->flags & TRACE_SYM_F_STRING) ?  TR_ASCII : 0;

		/* overrides */
		if (tab[i]->flags == TRACE_SYM_F_ANALOG) {
			flag = (TR_ANALOG_INTERPOLATED|
				TR_DEC|
				TR_ANALOG_FULLSCALE);
		}

		fprintf(fp, "@%x\n%s%s\n", flag|TR_RJUSTIFY, tab[i]->name,
			(tab[i]->flags == TRACE_SYM_F_U16) ? "[0:15]" : "");
	}
}

void write_savefile(const char *name)
{
	unsigned int ntraces;
	struct ltt_trace **tab;
	FILE *fp;

	ntraces = count_traces();
	if (ntraces == 0)
		return;

	tab = malloc(ntraces*sizeof(struct ltt_trace *));
	assert(tab);

	fp = fopen(name, "wb");
	if (fp == NULL)
		FATAL("cannot write savefile '%s': %s\n", name,
		      strerror(errno));

	INFO("writing SAV file '%s'...\n", name);

	print_group(TG_IRQ, "Interrupts", fp, tab);
	print_group(TG_MM, "Memory Management", fp, tab);
	print_group(TG_PROCESS, "Processes", fp, tab);

	free(tab);
	fclose(fp);
}
