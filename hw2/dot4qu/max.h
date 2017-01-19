// Written by Brian Team (dot4qu)
// Date: 9/28/16
// This is the header file corresponding to the max.cpp file 

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <string>
#include "semaphore.h"
#include "pthread.h"
#include "math.h"
#include "barrier.h"


#ifndef MAX_H
#define MAX_H

//simple macro for error checking
#define CHECK_ERR(x) if (x) {\
						printf("%s: Error at line %d with value %d \n", __func__, __LINE__, x); \
						return 0; \
					}

//struct to hold args for passing into threads
struct thread_args {
	std::vector<int> *input_nums; 	/* pointer to full input array */
	int start_idx;					/* pointer index throughout the input array to start from when comparing two numbers */
	int half_n;						/* holds # of threads, n/2 */
	int thread_num;					/* holds index of thread 0 - n/2 for checking what threads are still being used each iteration */
};

//thread function
int find_max(int a, int b);

#endif