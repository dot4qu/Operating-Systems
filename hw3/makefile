# Written by Brian Team (dot4qu)
# Date: 11/21/16
# This makefile is responsible for compiling and linking HW3, the FAT16 filesystem

CC=g++
DEPS=fat.cpp fat.h
OBJS=fat.o

%.o: %.cpp
	$(CC) -c -o $@ $< 

fat: $(OBJS)
	$(CC) -o $@ $^ 

clean:
	@rm -f *.o fat
