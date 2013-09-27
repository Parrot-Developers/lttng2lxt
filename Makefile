# Makefile for lttng2lxt

PREFIX  ?=/
INSTALL = install
CC	= gcc
CFLAGS	= -g -Wall -Wextra -Wno-unused-parameter -O3
HEADERS = lxt_write.h lttng2lxt.h
LIBS	= -lbabeltrace-ctf -lbabeltrace -lz -lbz2
PROGRAM = lttng2lxt

OBJS	= lxt_write.o lttng2lxt.o atag.o symbol.o modules.o savefile.o ctf.o \
	cpu_idle.o ev_kernel.o ev_task.o ev_userspace.o ev_syscall.o

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
