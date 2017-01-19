# Written by Brian Team (dot4qu)
# Date: 11/29/16
# This makefile is responsible for compiling and linking HWr, the simple FTP server implementation

CC=g++
CFLAGS=-m32
DEPS=my_ftpd.cpp my_ftpd.h
OBJS=my_ftpd.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

my_ftpd: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	@rm -f *.o my_ftpd