# Makefile for the pi files.
#
# CS 60, May 2018


CFLAGS = -Wall -pedantic -std=c11 -ggdb -I ../monitor $C
CC = gcc
MAKE = make

BITS := $(shell getconf LONG_BIT)
ifeq ($(BITS),64)
	CCFLAGS += -m32
endif

# for memory-leak tests
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all


########### messaging ##################
segment.o: segment.h 

.PHONY: valgrind clean

############## clean  ##########
clean:
	rm -rf *~ *.o *.dSYM .DS_Store
