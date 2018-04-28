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

int verbose;
int diag;
int gtkwave_parrot = 1;
int show_cpu_switch = 1;
int do_stats = 0;

static void link_gtkw_file(const char *tracefile, const char *savefile)
{
	int ret;
	char *gtkwfile;

	ret = asprintf(&gtkwfile, "%s.gtkw", tracefile);
	assert(ret > 0);
	unlink(gtkwfile);
	link(savefile, gtkwfile);
	free(gtkwfile);
}

static void usage(void)
{
	fprintf(stderr, "\nUsage: lttng2lxt [-v] [-d] [-c] [-s] [-a] [-S <stat mask>] [-e <exefile>] "
		"<lttng_trace_dir> [<outputfile> <savefile>]\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int c, ret;
	char *tracefile;
	char *outputfile, *savefile;
	int rebase_clock = 1;

	while ((c = getopt(argc, argv, "hvdcse:S:a")) != -1) {
		switch (c) {

		case 'e':
			atag_init(optarg);
			break;

		case 'v':
			verbose = 1;
			break;

		case 'd':
			diag = 1;
			break;

		case 'c':
			gtkwave_parrot = 1;
			break;

		case 's':
			show_cpu_switch = !show_cpu_switch;
			break;

		case 'S':
			do_stats = atoi(optarg);
			break;
		case 'a':
			rebase_clock = 0;
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}

	display_modules();

	if ((optind != argc-1) && (optind != argc-3))
		usage();

	tracefile = argv[optind];

	if (optind == argc-3) {
		outputfile = argv[optind+1];
		savefile = argv[optind+2];
	} else {
		/* make new names with proper extensions */
		if (tracefile[strlen(tracefile)-1] == '/')  /* strip last / */
			tracefile[strlen(tracefile)-1] = 0;
		ret = asprintf(&outputfile, "%s.fst", tracefile);
		assert(ret > 0);
		ret = asprintf(&savefile, "%s.sav", tracefile);
		assert(ret > 0);
	}

	save_dump_init(outputfile);

	/* do the actual work */
	scan_lttng_trace(tracefile, rebase_clock);

	/* create a savefile for GTKwave with comments, trace ordering, etc. */
	write_savefile(savefile);

	link_gtkw_file(tracefile, savefile);

	save_dump_close();

	fprintf(stdout, "%s: Generated '%s' file\n", argv[0], outputfile);

	if (optind != argc-3) {
		free(outputfile);
		free(savefile);
	}
	if (do_stats & STAT_IRQ)
		irq_stats();
	if (do_stats & STAT_SOFTIRQ)
		softirq_stats();

	unregister_modules();
	return 0;
}
