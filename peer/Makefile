# Makefile for the client_init files.
#
# CS 60, May 2018


FLAGS = #-DTEST
CCFLAGS = -Wall -pedantic -std=c11 -ggdb -pthread $(FLAGS) -I ../monitor
CC = gcc
MAKE = make

# get the operating system being built on
OS := $(shell uname -s)

BITS := $(shell getconf LONG_BIT)
ifeq ($(BITS),64)
	CCFLAGS += -m32
endif

TARGETS = peer
HEADERS = peer.h ../messaging/segment.h
OBJECTS = ../messaging/segment.o  ../filetable/filetable.o ../upload_download/download.o ../upload_download/upload.o
MONITORLIB= ../monitor/libmonitor.a

OSFLAGS := 
ifeq ($(OS), Darwin)
	OSFLAGS = -framework CoreServices
endif

########### startup ##################

all: $(TARGETS)

%.o: %.c $(HEADERS)
	$(CC) $(CCFLAGS) -c $< -o $@

$(TARGETS): %: %.c $(OBJECTS) $(MONITORLIB)
	$(CC) $(CCFLAGS) $(OSFLAGS) $(OBJECTS) $@.c -o $@ $(MONITORLIB) -lm

.PHONY: valgrind clean

############## clean  ##########
clean:
	rm -rf *.o  *~ $(TARGETS) $(OBJECTS) *.dSYM core vgcore*
