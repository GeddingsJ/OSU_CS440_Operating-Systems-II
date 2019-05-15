#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "mt19937ar.h"

#define MAX_THREAD 5
#define BUFFERSIZE 3
#define bit_RDRND (1 << 30) //RDRND flag

int x86system; //if x86system = 1, it supports rdrand

//item buffer: a struct with two numbers in it
struct Data{
	int thread_id;
	int local_using;
};

//struct Data buffer[BUFFERSIZE];
int locks_in_use[3] = {0};
int total = 0;
int lights_on = 0;
int max_capacity = 0;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condConsumer = PTHREAD_COND_INITIALIZER;

// Check if system supports rdrand or not
int checkSystem(){

	unsigned int eax = 0x01;
	unsigned int ebx;
	unsigned int ecx;
	unsigned int edx;

	//__volatile__ forces this code to execute and not be removed by an optimizer
	__asm__ __volatile__(
			"cpuid;"
			: "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
			: "a"(eax)
			);

	//if bit 30 of ecx register is set after calling CPUID, rdrand is supported
	// - Wikipedia
	if(ecx & bit_RDRND){
		x86system = 1;
		printf("RDRAND supported\n");
	}

	else{
		x86system = 0;
		printf("RDRAND not supported\n");
	}

	return x86system;

}

// Check the system, use correct rand function
int getRandNum(){

	int num = 0;

	// If system supports rdrand
	if(x86system==1){
		// Generate rand num with rdrand with inline assembly
		asm volatile ("rdrand %0":"=r"(num));
		num = abs(num);
	}

	else{
		// Generate rand positive num with merelene twister (genrand_int32 returns negatives as well)
		num = (int)genrand_int31();
		// Note: num is not within the range for producers and consumers yet
	}

	return num;
}

// Consumer thread function
void *cFunc(void *arg){

	// Obtain thread ID
	struct Data *resource = (struct Data*)arg;
	int thread_ID = (*resource).thread_id;
	int local_using = 0; 

	// Keep thread alive indefinitely
	while(1){
		//Initial resting period and cleaning of local variable.
		int initial_rest = getRandNum() % 15 + 5;
		printf("Thread %d resting for %d. \n", thread_ID, initial_rest);
		sleep(initial_rest);
		local_using = 0;	
	
		// Attempt a trylock for one of three available work spaces
		pthread_mutex_lock(&mutex1);
		if(locks_in_use[0] == 0 && max_capacity != 1){
			//Shared tracking resource, inform others that slot is filled.
			locks_in_use[0] = 1;
			printf("Thread %d locked to station 1 ", thread_ID);
			
			// If nobody else has arrived, state you are the first
			if(lights_on == 0){
				lights_on = 1;
			}
			
			// Global tracker for total is updated
			total++;
			
			// Variable indicating if thread has used a station this round
			local_using = 1;
			
			// Do 'work' for arbitrary amount of time.
			int sleeping = getRandNum()%10 + 3;
			printf("'working' for %d\n", sleeping);

			// If all three slots are filled - halt all new work
 			if(locks_in_use[1] == 1 && locks_in_use[2] == 1){
				printf("\n !! MAXIMUM OCCUPANCY !! \n\n");
				max_capacity = 1;
			}
			pthread_mutex_unlock(&mutex1);
			
			// 'Work'
			sleep(sleeping);

			pthread_mutex_lock(&mutex1);
			
			// Alert others that this station is now cleared
			printf("Thread %d at station 1 done.\n", thread_ID);
			locks_in_use[0] = 0;
			
			pthread_mutex_unlock(&mutex1);
		}
		pthread_mutex_unlock(&mutex1);
	
		// Repeat of station 1
		pthread_mutex_lock(&mutex1);
		if(locks_in_use[1] == 0 && max_capacity != 1){
			//printf("%d thread after lock use\n", thread_ID);
			if(local_using == 0){
				locks_in_use[1] = 1;
				printf("Thread %d locked to station 2 ", thread_ID);
				if(lights_on == 0){
					lights_on = 1;
				}
				total++;
				local_using = 1;
				int sleeping = getRandNum()%10 + 3;
				printf("'working' for %d\n", sleeping);
                                if(locks_in_use[0] == 1 && locks_in_use[2] == 1){
                                        printf("\n !! MAXIMUM OCCUPANCY !! \n\n");
                                        max_capacity = 1;
                                }
				pthread_mutex_unlock(&mutex1);
				sleep(sleeping);
				pthread_mutex_lock(&mutex1);
				printf("Thread %d at station 2 done.\n", thread_ID);
				locks_in_use[1] = 0;
			}
			pthread_mutex_unlock(&mutex1);
		}
		pthread_mutex_unlock(&mutex1);
		
		// Repeat of station 1
		pthread_mutex_lock(&mutex1);
		if(locks_in_use[2] == 0 && max_capacity != 1){
			if(local_using == 0){
				locks_in_use[2] = 1;
				printf("Thread %d locked to station 3 ", thread_ID);
				if(lights_on == 0){
					lights_on = 1;
				}
				total++;
				local_using = 1;
				int sleeping = getRandNum()%10 + 3;
				printf("'working' for %d\n", sleeping);
				if(locks_in_use[0] == 1 && locks_in_use[1] == 1){
                                        printf("\n !! MAXIMUM OCCUPANCY !! \n\n");
                                        max_capacity = 1;
                                }
				pthread_mutex_unlock(&mutex1);
				sleep(sleeping);
				pthread_mutex_lock(&mutex1);
				printf("Thread %d at station 3 done.\n", thread_ID);
				locks_in_use[2] = 0;
			}
			pthread_mutex_unlock(&mutex1);
		}
		pthread_mutex_unlock(&mutex1);
		
		// While work is ongoging and someone is still in a station
		while(total != 0 && lights_on == 1){
			// In the event of less than three threads - bypass and get to work
			if(MAX_THREAD < 3){
				total = 0;
				local_using = 0;
				lights_on = 0;
			}
			
			// If a thread that did work this round
			if(local_using == 1){
				pthread_mutex_lock(&mutex1);
				total--;
				local_using = 0;
				pthread_mutex_unlock(&mutex1);
				
				// If lockout occured, last one out releases locks
				if(total == 0 && lights_on == 1 && max_capacity == 1){
					printf("\nLAST OUT - ACCEPT NEW WORKERS \n\n");

					pthread_mutex_lock(&mutex1);
					lights_on = 0;
					max_capacity = 0;
					local_using = 2;
					pthread_mutex_unlock(&mutex1);

				// Last one is freed to work again
				}else if(local_using == 2){
					local_using = 0;
					break;
	
				// If a previous worker and capacity wasn't hit, get to work
				}else if(max_capacity == 0){
					break;
				}
			}
		}
		pthread_mutex_unlock(&mutex1);
	}
}

int main(int argc, char* argv[]){

	// Check if system supports rdrand
	checkSystem();
	
	int i; // Loop iterator
	int numThreads = MAX_THREAD; // Number of threads to create of each type
	pthread_t cons[numThreads]; // Consumer Array

	struct Data entity[MAX_THREAD];

	// pthread being given an ID	
	for(i=0;i<MAX_THREAD;i++){
		entity[i].thread_id = i;
	}
	
	// Seed Mersenne Twister if necessary
	if(x86system == 1)
	{
		unsigned long s = time(NULL);
		init_genrand(s);
	}

	// Generate threads
	for (i = 0; i < numThreads; i++) {
		pthread_create(&cons[i], NULL, cFunc, (void*) &entity[i]);
	}

	// Suspend main thread and wait to join finished threads
	for (i = 0; i < numThreads; i++) {
		pthread_join(cons[i], NULL);
	}

	return 0;
}
