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

#define HASHTAB_SIZE            (100000)
#define ADDR2LINE_MAX           (128)
#define ADDR_SIZE               (12)

int atag_enabled;
static const char *exefile;

static void snprintf_up(char **p, int size, const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vsnprintf(*p, size, fmt, ap);
	va_end(ap);
	assert(ret < size);
	*p += ret;
}

void atag_init(const char *name)
{
	int res;

	exefile = name;
	if (exefile) {
		/* create a new hash table */
		res = hcreate(HASHTAB_SIZE);
		assert(res);
		atag_enabled = 1;
	}
}

static void atag_enqueue_addr(uint32_t addr, int flush)
{
	static char cmdbuf[2*LINEBUF_MAX + ADDR2LINE_MAX*ADDR_SIZE];
	static char funcname[LINEBUF_MAX];
	static char basename[LINEBUF_MAX];
	static char tag[LINEBUF_MAX];
	static int index;
	static uint32_t addrbuf[ADDR2LINE_MAX];
	char key[16];
	ENTRY item, *rentry;
	char *ret, *str, *p;
	FILE *fp;
	int i;

	if (!exefile)
		return;

	if (!flush) {
		/* check that this address is not already queued */
		for (i = 0; i < index; i++) {
			if (addrbuf[i] == addr)
				/* already in */
				return;
		}
		addrbuf[index++] = addr;
	}

	if (!index || ((index < ADDR2LINE_MAX) && !flush))
		/* we can stop here */
		return;

	/* prepare command string */
	assert(index <= ADDR2LINE_MAX);
	p = cmdbuf;
	snprintf_up(&p, LINEBUF_MAX, "addr2line -C -f -s --exe=%s ", exefile);
	for (i = 0; i < index; i++)
		snprintf_up(&p, ADDR_SIZE, "0x%08x ", (uint32_t)addrbuf[i]);

	/* call addr2line now */
	fp = popen(cmdbuf, "r");
	if (!fp) {
		index = 0;
		return;
	}

	for (i = 0; i < index; i++) {

		ret = NULL;
		if (fgets(funcname, LINEBUF_MAX, fp) &&
		    fgets(basename, LINEBUF_MAX, fp)) {

			/* remove newline characters */
			str = strchr(funcname, '\n');
			if (str)
				*str = '\0';
			str = strchr(basename, '\n');
			if (str)
				*str = '\0';

			if (funcname[0] != '?')
				snprintf(tag, sizeof(tag), "%s() [%s]",
					 funcname, basename);
			else
				snprintf(tag, sizeof(tag), "0x%08x",
					 addrbuf[i]);

			ret = strdup(tag);
		}

		if (!ret) {
			fprintf(stderr, PFX "no symbol for addr 0x%08x: %s\n",
				(unsigned int)addrbuf[i], strerror(errno));
			continue;
		}

		/* add entry into hash table */
		snprintf(key, sizeof(key), "%08x", addrbuf[i]);
		item.key = strdup(key);
		assert(item.key);
		item.data = (void *)ret;
		rentry = hsearch(item, ENTER);
		assert(rentry);
	}
	pclose(fp);
	index = 0;
}

char *atag_get(uint32_t addr)
{
	char *ret = NULL;
	char key[16];
	ENTRY item, *fitem;

	if (exefile) {
		snprintf(key, sizeof(key), "%08x", addr);
		/* lookup address info in hash table */
		item.key = key;
		fitem = hsearch(item, FIND);
		if (fitem)
			ret = (char *)fitem->data;
	}
	return ret;
}

void atag_store(uint32_t addr)
{
	if (!atag_get(addr))
		atag_enqueue_addr(addr, 0);
}

void atag_flush(void)
{
	atag_enqueue_addr(0, 1);
}
