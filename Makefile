# Makefile for lttng2lxt

PREFIX  ?=/
INSTALL = install
CC	= gcc
LIBDIR  = export_apis
CFLAGS	= -g -Wall -Wextra -Wno-unused-parameter -O3 -I$(LIBDIR)
CFLAGS += -Wno-implicit-fallthrough
HEADERS = lttng2lxt.h $(LIBDIR)/fstapi.h $(LIBDIR)/fastlz.h $(LIBDIR)/lz4.h
LIBS	= -lbabeltrace-ctf -lbabeltrace -lz -lbz2
PROGRAM = lttng2lxt

OBJS	= lttng2lxt.o $(LIBDIR)/fstapi.o $(LIBDIR)/fastlz.o $(LIBDIR)/lz4.o \
	atag.o symbol.o modules.o savefile.o ctf.o \
	cpu_idle.o ev_kernel.o ev_task.o ev_user.o ev_syscall.o ev_signal.o

all: $(PROGRAM)

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROGRAM) : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	-rm -f $(OBJS) $(PROGRAM) *~

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -D $(PROGRAM) $(DESTDIR)$(PREFIX)/bin
