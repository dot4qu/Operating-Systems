// Written by Brian Team (dot4qu)
// Date: 9/28/16
// This is the header file for the Barrier class

#include "pthread.h"
#include "semaphore.h"
#include "stdio.h"

#ifndef BARRIER_H
#define BARRIER_H

class Barrier {

public:
	int val;
	int counter;
	sem_t *handshake;
	sem_t *mutex;
	sem_t *waiter;

	Barrier(int val);
	void wait();
};

#endif