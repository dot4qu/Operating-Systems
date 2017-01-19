 # Written by Brian Team (dot4qu)
# Date: 9/27/16
# This program acts as a bare-bones shell implementation including file I/O redirection and pipes 


CC=gcc
CFLAGS=
DEPS=msh.h

%.o:%.c $(DEPS)
	$(CC) -c -o $@ $<

msh: msh.o 
	$(CC) -o msh msh.o 