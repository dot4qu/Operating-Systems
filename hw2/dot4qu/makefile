# Written by Brian Team (dot4qu)
# Date: 9/28/16
# This makefile is responsible for compiling and linking HW2, the max finding process synchronization problem

CC=g++
CFLAGS=-pthread -ggdb
DEPS=max.cpp barrier.cpp max.h barrier.h
OBJS=max.o barrier.o

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

max: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	@rm -f *.o max
