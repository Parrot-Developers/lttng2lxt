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
#include "lxt_write.h"

static struct lt_trace *lt;
static struct ltt_trace *head;
static int symbol_flushed;
static const char *lxt_name;

void symbol_clean_name(char *name)
{
	char *pname = name;

	/* replace . by _ */
	while ((pname = strchr(pname, '.'))) {
		*pname = '_';
		pname++;
	}
}

static void insert_symbol(struct ltt_trace *tr)
{
	uint32_t flags;
	int bits = 0;

	tr->sym = lt_symbol_find(lt, tr->name);
	if (!tr->sym) {
		bits = 0;
		if ((tr->flags == TRACE_SYM_F_ADDR) && !atag_enabled)
			tr->flags = TRACE_SYM_F_INTEGER;

		switch (tr->flags) {
		case TRACE_SYM_F_BITS:
			flags = LT_SYM_F_BITS;
			break;
		case TRACE_SYM_F_U16:
			bits = 15;
		case TRACE_SYM_F_INTEGER:
			flags = LT_SYM_F_INTEGER;
			break;
		case TRACE_SYM_F_STRING:
			flags = LT_SYM_F_STRING;
			break;
		case TRACE_SYM_F_ANALOG:
			flags = LT_SYM_F_DOUBLE;
			break;
		case TRACE_SYM_F_ADDR:
			flags = LT_SYM_F_STRING;
			break;
		default:
			assert(0);
		}
		tr->sym = lt_symbol_add(lt, tr->name, 0, 0, bits, flags);
		assert(tr->sym);
	}
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

void symbol_flush(void)
{
	struct ltt_trace *tr;

	for (tr = trace_head(); tr; tr = tr->next)
		insert_symbol(tr);

	symbol_flushed = 1;
}

void emit_trace(struct ltt_trace *tr, union ltt_value value, ...)
{
	va_list ap;
	static char linebuf[LINEBUF_MAX];

	assert(tr->sym);
	tr->emitted = 1;
	switch (tr->flags) {

	case TRACE_SYM_F_BITS:
		lt_emit_value_bit_string(lt, tr->sym, 0, value.state);
		break;

	case TRACE_SYM_F_U16:
		assert(value.data <= 0xffff);
	case TRACE_SYM_F_INTEGER:
		lt_emit_value_int(lt, tr->sym, 0, value.data);
		break;

	case TRACE_SYM_F_ANALOG:
		lt_emit_value_double(lt, tr->sym, 0, value.dataf);
		break;

	case TRACE_SYM_F_STRING:
		va_start(ap, value);
		vsnprintf(linebuf, LINEBUF_MAX, value.format, ap);
		va_end(ap);
		lt_emit_value_string(lt, tr->sym, 0, linebuf);
		break;

	case TRACE_SYM_F_ADDR:
		lt_emit_value_string(lt, tr->sym, 0, atag_get(value.data));
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
	lxttime_t timeval;
	static lxttime_t oldtimeval;

	timeval = (lxttime_t)(1000000000.0*clock);
	if (timeval < oldtimeval) {
		DIAG("negative time offset @%lld: %d !\n", oldtimeval,
		     (int)((int64_t)timeval-(int64_t)oldtimeval));
		timeval = oldtimeval + 1;
	} else if (timeval == oldtimeval) {
		return;
	}

	oldtimeval = timeval;
	lt_set_time64(lt, timeval);
}

void save_dump_init(const char *lxtfile)
{
	lt = lt_init(lxtfile);
	assert(lt);

	lxt_name = lxtfile;

	/* set time resolution */
	lt_set_timescale(lt, -9);
	lt_set_initial_value(lt, 'z');
}

void save_dump_close(void)
{
	INFO("writing LXT file '%s'...\n", lxt_name);
	lt_close(lt);
}
