# Makefile for file upload/download files

CC = gcc
CCFLAGS = -Wall -pedantic -pthread -std=c11 -ggdb -I ../monitor
MAKE = make

BITS := $(shell getconf LONG_BIT)
ifeq ($(BITS),64)
	CCFLAGS += -m32
endif

HEADERS = ../monitor/fileinfo.h ../filetable/filetable.h ../peer/peer.h upload_download.h
OBJECTS = upload.o download.o

all: $(OBJECTS)

%.o: %.c $(HEADERS)
	$(CC) $(CCFLAGS) -c $< -o $@

.PHONY: valgrind clean

clean:
	rm -rf *.o  *~ $(OBJECTS) *.dSYM core vgcore*
