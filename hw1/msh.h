/* 
Written by Brian Team (dot4qu)
Date: 9/27/16
This program acts as a bare-bones shell implementation including file I/O redirection and pipes 
*/


#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#define DEBUG 0

#ifndef MSH_H
#define MSH_H

char** parse_input(char *input, int *pipe_count, int *invalid_flag, int *dup_stderr);
int test_args(char **args);

#endif