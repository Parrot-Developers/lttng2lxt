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

#define MAX_USER_EVENTS		32
#define MAX_KERNEL_EVENTS	32

static struct ltt_trace user_trace_g;
static struct ltt_trace kernel_trace_g;
static struct ltt_trace user_traces[MAX_USER_EVENTS];
static struct ltt_trace kernel_traces[MAX_KERNEL_EVENTS];

static void user_event_start_process(const char *modname, int pass,
				     double clock, int cpu, void *args)
{
	int num = (int)get_arg_i64(args, "event_start");

	if (pass == 1) {
		if (num < (int)(sizeof(user_traces) /
				sizeof(user_traces[0])) && num >= 0)
			init_trace(&user_traces[num],
				   TG_USER,
				   1 + 0.1 * num,
				   TRACE_SYM_F_BITS,
				   "user event %d",
				   num);
	}

	if (pass == 2) {
		if (num < (int)(sizeof(user_traces) /
				sizeof(user_traces[0])) && num >= 0)
			emit_trace(&user_traces[num], (union ltt_value)LT_S0);
	}
}
MODULE2(user, event_start);

static void user_event_stop_process(const char *modname, int pass,
				    double clock, int cpu, void *args)
{
	int num = (int)get_arg_i64(args, "event_stop");

	if (pass == 1) {
		if (num < (int)(sizeof(user_traces) /
				sizeof(user_traces[0])) && num >= 0)
			init_trace(&user_traces[num],
				   TG_USER,
				   1 + 0.1 * num,
				   TRACE_SYM_F_BITS,
				   "user event %d",
				   num);
	}

	if (pass == 2) {
		if (num < (int)(sizeof(user_traces) /
				sizeof(user_traces[0])) && num >= 0)
			emit_trace(&user_traces[num], (union ltt_value)LT_IDLE);
	}
}
MODULE2(user, event_stop);

static void user_message_process(const char *modname, int pass,
				 double clock, int cpu, void *args)
{
	const char *str = get_arg_str(args, "message");

	if (pass == 1)
		init_trace(&user_trace_g, TG_USER, 1,
			   TRACE_SYM_F_STRING, "user event");

	if (pass == 2)
		emit_trace(&user_trace_g, (union ltt_value)"%s", str);
}
MODULE2(user, message);

static void user_kevent_start_process(const char *modname, int pass,
				      double clock, int cpu, void *args)
{
	int num = (int)get_arg_i64(args, "event_start");

	if (pass == 1) {
		if (num < (int)(sizeof(kernel_traces) /
				sizeof(kernel_traces[0])) && num >= 0)
			init_trace(&kernel_traces[num],
				   TG_USER,
				   0.1 * num,
				   TRACE_SYM_F_BITS,
				   "kernel event %d",
				   num);
	}

	if (pass == 2) {
		if (num < (int)(sizeof(kernel_traces) /
				sizeof(kernel_traces[0])) && num >= 0)
			emit_trace(&kernel_traces[num], (union ltt_value)LT_S0);
	}
}
MODULE(user_kevent_start);

static void user_kevent_stop_process(const char *modname, int pass,
				     double clock, int cpu, void *args)
{
	int num = (int)get_arg_i64(args, "event_stop");

	if (pass == 1) {
		if (num < (int)(sizeof(kernel_traces) /
				sizeof(kernel_traces[0])) && num >= 0)
			init_trace(&kernel_traces[num],
				   TG_USER,
				   0.1 * num,
				   TRACE_SYM_F_BITS,
				   "kernel event %d",
				   num);
	}

	if (pass == 2) {
		if (num < (int)(sizeof(kernel_traces) /
				sizeof(kernel_traces[0])) && num >= 0)
			emit_trace(&kernel_traces[num],
				   (union ltt_value)LT_IDLE);
	}
}
MODULE(user_kevent_stop);

static void user_kmessage_process(const char *modname, int pass,
				  double clock, int cpu, void *args)
{
	const char *str = get_arg_str(args, "message");

	if (pass == 1)
		init_trace(&kernel_trace_g, TG_USER, 0,
			   TRACE_SYM_F_STRING, "kernel event");

	if (pass == 2)
		emit_trace(&kernel_trace_g, (union ltt_value)"%s", str);
}
MODULE(user_kmessage);
