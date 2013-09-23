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
int gtkwave_parrot;

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
	fprintf(stderr, "\nUsage: lttng2lxt [-v] [-d] [-c] [-e <exefile>] "
		"<lttng_trace_dir> [<lxtfile> <savefile>]\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int c, ret;
	char *tracefile;
	char *lxtfile, *savefile;

	while ((c = getopt(argc, argv, "hvdce:")) != -1) {
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

		case 'h':
		default:
			usage();
			break;
		}
	}

	if ((optind != argc-1) && (argc != argc-3))
		usage();

	tracefile = argv[optind];

	if (optind == argc-3) {
		lxtfile = argv[optind+1];
		savefile = argv[optind+2];
	} else {
		/* make new names with proper extensions */
		if (tracefile[strlen(tracefile)-1] == '/')  /* strip last / */
			tracefile[strlen(tracefile)-1] = 0;
		ret = asprintf(&lxtfile, "%s.lxt", tracefile);
		assert(ret > 0);
		ret = asprintf(&savefile, "%s.sav", tracefile);
		assert(ret > 0);
	}

	modules_init();
	save_dump_init(lxtfile);

	/* do the actual work */
	scan_lttng_trace(tracefile);

	/* create a savefile for GTKwave with comments, trace ordering, etc. */
	write_savefile(savefile);

	link_gtkw_file(tracefile, savefile);

	save_dump_close();

	if (optind != argc-3) {
		free(lxtfile);
		free(savefile);
	}

	return 0;
}
