# Makefile for file monitoring library/helpers

CC = gcc
CCFLAGS = -Wall -pedantic -pthread -std=c11 -ggdb -D_GNU_SOURCE

# get the operating system being built on
OS := $(shell uname -s)

# only set m32 flag if on a 64 system
BITS := $(shell getconf LONG_BIT)
ifeq ($(BITS),64)
	CCFLAGS += -m32 -D_FILE_OFFSET_BITS=64
endif


LIB = libmonitor.a

TARGETS = test
HEADERS = monitor.h fileobserver.h fileevent.h fileinfo.h eventqueue.h fileset.h
OBJECTS = monitor.o fileobserver.o fileevent.o fileinfo.o eventqueue.o fileset.o

observer := fileobserver.c
OSFLAGS := 
ifeq ($(OS), Darwin)
	observer := fileobserver-mac.c
	OSFLAGS = -framework CoreServices
endif

all: $(TARGETS) lib

lib: $(LIB)


$(LIB): $(TARGETS)
	ar cr $(LIB) $(OBJECTS) eventqueue.o

# separate target for this, since it's source name is a separate var
fileobserver.o: fileobserver.h $(observer)
	$(CC) $(CCFLAGS) -c $(observer) -o $@ 

%.o: %.c $(HEADERS)
	$(CC) $(CCFLAGS) -c $< -o $@

$(TARGETS): %: %.c $(OBJECTS)
	$(CC) $(CCFLAGS) $(OBJECTS) $(OSFLAGS) $@.c -o $@

.PHONY: lib clean

clean:
	rm -rf *.o  *~ $(TARGETS) *.dSYM core vgcore* libmonitor.a

