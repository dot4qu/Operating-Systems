// Written by Brian Team (dot4qu)
// Date: 9/28/16
// This .cpp file is responsible for implementing the barrier class to cause the threads to wait

#include "barrier.h"

//constructor
Barrier::Barrier(int value) {
	this->val = value;
	this->counter = 0;
	this->mutex = new sem_t;
	this->waiter = new sem_t;
	this->handshake = new sem_t;
	sem_init(this->mutex, 0, 1);
	sem_init(this->waiter, 0, 0);
	sem_init(this->handshake, 0, 0);
}

void Barrier::wait() {
	//wait for mutex 
	sem_wait(this->mutex);
	//pre increment counter for # threads waiting on barrier
	this->counter++;
	//are we the very last thread to enter the barrier?
	if (counter == val) {
		//last thread into barrier, release everyone. i < val - 1 because we want to awake val - 1 other threads, we never slept
		for (int i = 0; i < (val - 1); i++) {
			//signal the waiter to release a waiting thread from down below
			sem_post(this->waiter);
			//wait on the handshake so we know only a single thread crosses the waiter-wait to then signal our handshake
			sem_wait(this->handshake);
		}
		// reset counter to let threads start to pile up again
		counter = 0;
		//awaken mutex for others to enter again
		sem_post(this->mutex);
		//were done, get back to work
		return;
	}
		//always give back the mutex lock
	sem_post(this->mutex);
	//implied else clause which means we're not the last one in, we need to wait on the waiter
	sem_wait(this->waiter);
	//signal the handshake so release loop above knows a signal thread has passed the waiter and is restarted
	sem_post(this->handshake);
}