# Makefile for file monitoring library/helpers

CC = gcc
CCFLAGS = -Wall -pedantic -pthread -std=c11 -ggdb -I ../monitor

BITS := $(shell getconf LONG_BIT)
ifeq ($(BITS),64)
	CCFLAGS += -m32
endif

TARGETS = tracker
HEADERS = tracker.h peertable.h ../messaging/segment.h ../filetable/filetable.h 
OBJECTS = peertable.o ../messaging/segment.o ../filetable/filetable.o 
MONITORLIB= ../monitor/libmonitor.a

all: $(TARGETS)

%.o: %.c $(HEADERS)
	$(CC) $(CCFLAGS) -c $< -o $@

$(TARGETS): %: %.c $(OBJECTS) $(HEADERS)
	$(CC) $(CCFLAGS) $(OBJECTS) $@.c -o $@ $(MONITORLIB)

.PHONY: valgrind clean

clean:
	rm -rf *.o  *~ $(TARGETS) *.dSYM core vgcore*
