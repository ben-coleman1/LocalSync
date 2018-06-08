# Makefile for the network layer 
#
# CS 60, March 2018


PROGS = #filetabletest
LIBS = -pthread
LLIBSF = ../monitor/
LLIBS = $(LLIBSF)libmonitor.a

CFLAGS = -Wall -pedantic -std=c11 -ggdb -I$(LLIBSF)
CC = gcc
MAKE = make

BITS := $(shell getconf LONG_BIT)
ifeq ($(BITS),64)
	CFLAGS += -m32
endif

# for memory-leak tests
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all

all: $(PROGS)

#filetabletest: filetable.o filetabletest.o $(LLIBS)
#$(CC) $(CFLAGS) $^ $(LIBS) -o $@

##### source dependencies
filetable.o: filetable.h

.PHONY: valgrind clean

############## clean  ##########
clean:
	rm -rf *~ *.o *.dSYM .DS_Store
	rm -rf $(PROGS)

