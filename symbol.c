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
#include <fstapi.h>

static void *fst_ctx;
static struct ltt_trace *head;
static int symbol_flushed;
static const char *out_name;

void symbol_clean_name(char *name)
{
	char *pname = name;

	/* replace . by _ */
	while ((pname = strchr(pname, '.'))) {
		*pname = '_';
		pname++;
	}
}

struct fst_symbol_list {
	fstHandle symbol;
	char *name;
	struct fst_symbol_list *next;
};

char *get_fst_clean_name(const char *name)
{
	static char clean_name[1024];
	char *name_ptr;

	name_ptr = (char *)name;
	if (!strncmp(name_ptr, "proc.state.", strlen("proc.state.")))
		name_ptr += strlen("proc.state.");
	if (!strncmp(name_ptr, "proc.info.", strlen("proc.info.")))
		name_ptr += strlen("proc.info.");
	strncpy(clean_name, name_ptr, sizeof(clean_name));
	clean_name[sizeof(clean_name) - 1] = '\0';
	for (name_ptr = clean_name; *name_ptr != '\0'; name_ptr++) {
		if (*name_ptr == ' ')
			*name_ptr ='_';
		if (*name_ptr == '[')
			*name_ptr ='{';
		if (*name_ptr == ']')
			*name_ptr ='}';
	}
	return clean_name;
}

void insert_symbol(struct ltt_trace *tr)
{
	int vartype;
	int len = 1;
	struct fst_symbol_list *tmp;

	if (tr->fst_handle != 0)
		return;

	tmp = (struct fst_symbol_list *)calloc(1, sizeof(struct fst_symbol_list));
	if (tmp == NULL) {
		fprintf(stderr, "Allocation error creatinf fymbols list\n");
		return;
	}
	tmp->name = strdup(tr->name);
	tr->fst_name = strdup(get_fst_clean_name(tr->name));
	switch (tr->flags) {
	case TRACE_SYM_F_BITS:
		vartype = FST_VT_VCD_WIRE;
		break;
	case TRACE_SYM_F_INTEGER:
		vartype = FST_VT_VCD_REAL;
		len = 4;
		break;
	case TRACE_SYM_F_STRING:
		vartype = FST_VT_GEN_STRING;
		len = 0; /* use fstWriterEmitVariableLengthValueChange */
		break;
	case TRACE_SYM_F_ANALOG:
		vartype = FST_VT_VCD_REAL;
		break;
	case TRACE_SYM_F_ADDR:
		vartype = FST_VT_VCD_INTEGER;
		len = 4;
		break;
	default:
		assert(0);
	}

	tmp->symbol = fstWriterCreateVar(fst_ctx, vartype,
					 FST_VD_IMPLICIT,
					 len, tr->fst_name, 0);
	tr->fst_handle = tmp->symbol;
	if (tr->fst_handle == 0)
		fprintf(stderr, "Failed to add fst symbol for '%s'\n", tr->name);
}

void init_trace(struct ltt_trace *tr,
		enum trace_group group,
		double pos,
		uint32_t flags,
		const char *fmt, ...)
{
	va_list ap;
	static char linebuf[LINEBUF_MAX];

	if (tr->name == NULL) {

		tr->flags = flags;
		tr->group = group;
		tr->pos = pos;
		tr->next = head;
		head = tr;

		va_start(ap, fmt);
		vsnprintf(linebuf, LINEBUF_MAX, fmt, ap);
		va_end(ap);

		tr->name = strdup(linebuf);

		INFO("adding trace '%s' group=%d pos=%g\n", linebuf, group,
		     pos);
		if (symbol_flushed)
			/* XXX hack late symbol flush */
			insert_symbol(tr);
	}
}

void refresh_name(struct ltt_trace *tr,
		  const char *fmt, ...)
{
	va_list ap;
	static char linebuf[LINEBUF_MAX];

	assert(tr->name);

	va_start(ap, fmt);
	vsnprintf(linebuf, LINEBUF_MAX, fmt, ap);
	va_end(ap);

	if (strcmp(tr->name, linebuf)) {
		INFO("refreshing %s -> %s\n", tr->name, linebuf);
		free((char *)tr->name);
		tr->name = strdup(linebuf);
	}
}

void insert_amm_symbols(unsigned int group)
{
	struct ltt_trace *tr;
	for (tr = trace_head(); tr; tr = tr->next) {
		if (tr->group == group)
			insert_symbol(tr);
	}
}

void symbol_flush(void)
{
	fstWriterSetScope(fst_ctx, FST_ST_VCD_CLASS, "All Info", NULL);
	insert_amm_symbols(TG_NONE);
	fstWriterSetUpscope(fst_ctx);
	fstWriterSetScope(fst_ctx, FST_ST_VCD_PACKAGE, "Global Info", NULL);
	insert_amm_symbols(TG_GLOBAL);
	fstWriterSetUpscope(fst_ctx);
	fstWriterSetScope(fst_ctx, FST_ST_VHDL_IF_GENERATE, "Interrupts", NULL);
	insert_amm_symbols(TG_IRQ);
	fstWriterSetUpscope(fst_ctx);
	fstWriterSetScope(fst_ctx, FST_ST_VCD_PACKAGE, "Memory Management", NULL);
	insert_amm_symbols(TG_MM);
	fstWriterSetUpscope(fst_ctx);
	fstWriterSetScope(fst_ctx, FST_ST_VCD_STRUCT, "User Info", NULL);
	insert_amm_symbols(TG_USER);
	fstWriterSetUpscope(fst_ctx);
	fstWriterSetScope(fst_ctx, FST_ST_VCD_TASK, "Processes", NULL);
	insert_amm_symbols(TG_PROCESS);
	fstWriterSetUpscope(fst_ctx);
	symbol_flushed = 1;
}

void symbol_fst_initvalues(void)
{
       struct ltt_trace *tr;
       for (tr = trace_head(); tr; tr = tr->next)
               if (tr->flags == TRACE_SYM_F_BITS)
                       fstWriterEmitValueChange(fst_ctx, tr->fst_handle, "z");
}

void emit_trace(struct ltt_trace *tr, union ltt_value value, ...)
{
	va_list ap;
	static char linebuf[LINEBUF_MAX];
	static int first_emit = 1;

	if (tr->fst_handle == 0) {
		fprintf(stderr, "No symbol for '%s'\n", tr->name);
		return;
	}

	if (first_emit) {
		/* First emit: init values at first */
		first_emit = 0;
		symbol_fst_initvalues();
	}
	tr->emitted = 1;
	switch (tr->flags) {

	case TRACE_SYM_F_BITS:
		fstWriterEmitValueChange(fst_ctx, tr->fst_handle, value.state);
		break;

	case TRACE_SYM_F_U16:
		assert(value.data <= 0xffff);
	case TRACE_SYM_F_INTEGER:
		fstWriterEmitValueChange(fst_ctx, tr->fst_handle, &value.data);
		break;

	case TRACE_SYM_F_ANALOG:
		fstWriterEmitValueChange(fst_ctx, tr->fst_handle, &value.dataf);
		break;

	case TRACE_SYM_F_STRING:
		va_start(ap, value);
		vsnprintf(linebuf, LINEBUF_MAX, value.format, ap);
		va_end(ap);
		fstWriterEmitVariableLengthValueChange(fst_ctx, tr->fst_handle,
						       linebuf, strlen(linebuf));
		break;

	case TRACE_SYM_F_ADDR:
		fstWriterEmitValueChange(fst_ctx, tr->fst_handle,
						 atag_get(value.data));
		break;
	default:
		assert(0);
	}
}

struct ltt_trace *trace_head(void)
{
	return head;
}

void emit_clock(double clock)
{
	uint64_t timeval;
	static uint64_t oldtimeval;

	timeval = (uint64_t)(1000000000.0*clock);
	if (timeval < oldtimeval) {
		DIAG("negative time offset @%lu: %lu !\n", oldtimeval,
		     (int64_t)timeval - oldtimeval);
		timeval = oldtimeval + 1;
	} else if (timeval == oldtimeval) {
		return;
	}
	oldtimeval = timeval;
	fstWriterEmitTimeChange(fst_ctx, timeval);
}

void save_dump_init(const char *outfile)
{
	out_name = outfile;

	fst_ctx = fstWriterCreate(outfile, 1);
	assert(fst_ctx);
	fstWriterSetPackType(fst_ctx, FST_WR_PT_LZ4);
	/* 0 is normal, 1 does the repack (via fstapi) at end */
	fstWriterSetRepackOnClose(fst_ctx, 0);
	/* 0 is is single threaded, 1 is multi-threaded */
	fstWriterSetParallelMode(fst_ctx, 0);
	fstWriterEmitDumpActive(fst_ctx, 1);
}

void save_dump_close(void)
{
	INFO("writing output file '%s'...\n", out_name);
	fstWriterEmitDumpActive(fst_ctx, 0);
	fstWriterClose(fst_ctx);
}
