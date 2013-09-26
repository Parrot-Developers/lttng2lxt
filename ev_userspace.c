/**
 * LTTng to GTKwave trace conversion
 *
 * Authors:
 * Adrien Charruel <adrien.charruel@parrot.com>
 *
 * Copyright (C) 2013 Parrot S.A.
 */

#include "lttng2lxt.h"
#include "lttng.h"

#define MAX_USER_EVENTS     32

static struct ltt_trace trace_g;
static struct ltt_trace traces[MAX_USER_EVENTS];

static void userspace_event_start_process(const char *modname, int pass,
					  double clock, int cpu, void *args)
{
	int num = (int)get_arg_i64(args, "event_start");

	if (pass == 1) {
		if (num < (int)(sizeof(traces)/sizeof(traces[0])) && num >= 0)
			init_trace(&traces[num], TG_PROCESS, 0.1+0.1*num,
				   TRACE_SYM_F_BITS, "user event %d", num);
	}

	if (pass == 2) {
		if (num < (int)(sizeof(traces)/sizeof(traces[0])) && num >= 0) {
			emit_trace(&traces[num], (union ltt_value)LT_S0);
		}
	}
}
MODULE2(userspace, event_start);

static void userspace_event_stop_process(const char *modname, int pass,
					 double clock, int cpu, void *args)
{
	int num = (int)get_arg_i64(args, "event_stop");

	if (pass == 1) {
		if (num < (int)(sizeof(traces)/sizeof(traces[0])) && num >= 0)
			init_trace(&traces[num], TG_PROCESS, 0.1+0.1*num,
				   TRACE_SYM_F_BITS, "user event %d", num);
	}

	if (pass == 2) {
		if (num < (int)(sizeof(traces)/sizeof(traces[0])) && num >= 0) {
			emit_trace(&traces[num], (union ltt_value)LT_IDLE);
		}
	}
}
MODULE2(userspace, event_stop);

static void userspace_message_process(const char *modname, int pass,
				      double clock, int cpu, void *args)
{
	const char * str = get_arg_str(args, "message");

	if (pass == 1)
		init_trace(&trace_g, TG_PROCESS, 0.1,
			   TRACE_SYM_F_STRING, "user event");

	if (pass == 2)
		emit_trace(&trace_g, (union ltt_value)"%s", str);

}
MODULE2(userspace, message);
