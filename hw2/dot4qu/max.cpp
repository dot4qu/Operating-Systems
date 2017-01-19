// Written by Brian Team (dot4qu)
// Date: 9/28/16
// This .cpp file is responsible for implementing the spawning and running of the max algorithm 

#include "max.h"

//global semaphore declaration to use between main and thread func
sem_t main_sem;
//global barrier declaration to init in main and wait on in thread func
Barrier *barrier;

/* this function finds the max of two elements and stores it in the position held by its leftmost pointer.
	After each max is found, 'round' is incremeneted to handle different indexing and offsetting as the
	threads parse more and more values. Each thread waits on the barrier class after finding a max so that no
	threads will get ahead of each other and grab the wrong values from the array that havent been set yet. 
	Once completely finished, the pthreads must call exit. Since they're detached, they can't be joined in main. */
void* find_max(void *input) {
	int round;									/* keeps track of of how far to offset comparison pointer for how many previous sets of comparisons weve done */
	int half_n;									/* half of total numbers input initially. serves to let us know when to stop comparing */
	int start_idx;								/* index for lefthand pointer to begin comparison at. thread 0 will be 0, thread 1 will be 2, etc */
	std::vector<int> *input_nums;				/* pointer to original input vector whose addr was passed in args */
	int thread_num;								/* holds current thread number to tell if this is one that's still doing work (lower halves) or is unneeded and can skip to wait */
	std::vector<int>::iterator start_iter;		/*iterator that points to the more left value to be swapped*/
	std::vector<int>::iterator offset_iter;		/*iterator that points to the more right value to be swapped*/

	//pthread_detach(pthread_self());
	
	//cast input args
	thread_args *args = (thread_args*) input;
	//begin round as 1 and double each loop iteration
	round = 1;	
	//assign local vars from struct	
	half_n = args->half_n;		
	input_nums = args->input_nums;	
	thread_num = args->thread_num;	
	start_idx = args->start_idx;

	//while still maxes left to compute ( round < n/2 )
	while (round <= half_n) {
		//for this is true for lower and lower portions of overall thread array each iteration. others skip over and just wait
		if (thread_num <= (half_n / round - 1)) {
			//take element at start_idx and compare to round*2 to see if we need to swap to get bigger on left
			if (input_nums->at(start_idx) > input_nums->at(start_idx + round)) {
				//do nothing, bigger element is already in lefthand index
			} else {
				//bigger element is on right, need to swap
				//setting iters to beginning element and second element compared to swap
				start_iter = input_nums->begin() + start_idx;
				offset_iter = input_nums->begin() + start_idx + round;
				iter_swap(start_iter, offset_iter);
			}

			//sem is decremented by every thread still doing work (n - 1) threads total
			sem_wait(&main_sem);
		}
		//double round var to ensure were indexing far enough next iter
		round *= 2;
		//double start_idx var to use earlier threads to keep moving down the line and comparing deeper results
		start_idx *= 2;

		//wait on barrier for all other threads to perform their swaps
		barrier->wait();

	}
	//finished with all comparisons, max num is in input_nums[0]. need to exit since we're detached, otherwise we'd float
	pthread_exit(NULL);
}


int main(int argc, char *argv[]) {
	std::vector<int> input_numbers;			/*vector to hold all of the input values*/
	std::string input_char;					/*string to hold char representation of input numbers*/
	int n = 0;								/*hold total number of ints input*/
	int half_n = 0;							/*number to hold the total number of threads we need to create*/
	pthread_t *thread_ids;					/*array to hold the thread id's of all n/2 threads*/
	pthread_attr_t default_attrs;			/*single attrs object that we can use to initialize all of our threads. */
	thread_args *args_arr;					/*array to hold each of the args so once we call create on the threads we have the return values*/
	int i = 0;								/*multiuse iterator for loops*/
	int args_arr_idx = 0;					/*holds slower iterating value for populating args arr with doubly moving value through the input numbers*/
	int err = 0;							/* var used for checking return values of functions for errors*/
	int sem_val = 0;						/*holds value still in semaphore. main checks this after firing off every thread to know when all calculations are done */


	//keeps pulling in lines (chars) until EOF or line is empty
	while ( getline(std::cin, input_char) && !input_char.empty()) {
		input_numbers.push_back(atoi(input_char.c_str())); 
		n++;
	}
	int hi = input_numbers.max_size();

	//need to take n/2 for since it currently equals total number of values entered
	half_n = n / 2;

	//creating new barrier object holding a value of n/2 for all of the threads
	barrier = new Barrier(half_n);

	//allocating space for every thread necessary
	thread_ids = new pthread_t[half_n];

	//instantiating default attrs
    CHECK_ERR( pthread_attr_init(&default_attrs) );
	//CHECK_ERR( pthread_attr_setdetachstate(&default_attrs, PTHREAD_CREATE_DETACHED) );
	pthread_attr_setstacksize(&default_attrs, 65536);

    CHECK_ERR( sem_init(&main_sem, 0, n - 1) );

	//initializing array with n/2 args
	args_arr = new thread_args[half_n];

	//populating each arg with pointer to vector and its corresponding start location
	for (i = 0, args_arr_idx = 0; args_arr_idx < half_n; i += 2, args_arr_idx++) {
		//initialize new args
		thread_args *new_args = new thread_args;
		//set values as we iterate down the input array
		new_args->input_nums = &input_numbers;
		new_args->start_idx = i;
		new_args->half_n = half_n;
		new_args->thread_num = args_arr_idx;
		//adding newly created args into array
		args_arr[args_arr_idx] = *new_args;
	}

	//looping through and creating n/2 threads
	for (i = 0; i < half_n; i++) {
		err = pthread_create(
							&thread_ids[i],
							&default_attrs,
							find_max,
							&args_arr[i] 
							);
		pthread_detach(thread_ids[i]);
		CHECK_ERR(err);
	}

	//loops through checking sem value until it's been 'wait'ed down to zero by the threads. Ensures main halts until all calcs are done
	sem_getvalue(&main_sem, &sem_val);
	while (sem_val > 0) {
		sem_getvalue(&main_sem, &sem_val);
	}

	printf("Max is %d\n", input_numbers.at(0));

	//delete/deallocate alloced mem
	free(barrier);
	free(thread_ids);
	free(args_arr);
	return 0;
}