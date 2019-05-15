#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "mt19937ar.h"

#define BUFFERSIZE 32
#define bit_RDRND (1 << 30) //RDRND flag

int x86system; //if x86system = 1, it supports rdrand

//compile with:
//gcc concurrency1-10.c -l pthread
//you have to link to the pthread library

//item buffer: a struct with two numbers in it
struct Data{
	int number;
	int wait_time;
};

struct Data buffer[BUFFERSIZE];
int bufferPointer = 0;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condConsumer = PTHREAD_COND_INITIALIZER;
pthread_cond_t condProducer = PTHREAD_COND_INITIALIZER;

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

	// Keep thread alive indefinitely
	while(1){
		// Lock mutex to gain exclusive access to the buffer
		pthread_mutex_lock(&mutex1);

		// While the buffer is empty...
		while(bufferPointer <= 0){

			// Print a message and wait on the consumer condition variable
			printf("Buffer empty, waiting for signal.\n");
			pthread_cond_wait(&condConsumer, &mutex1);
		}

		// Gather data and remove it from buffer
		int wTime = buffer[bufferPointer - 1].wait_time;
		int bNum = buffer[bufferPointer - 1].number;

		bufferPointer--;

		// Do simulated work
		printf("Consumed: Number %d, Wait Time %d\n", bNum, wTime);
		sleep(wTime);

		// Signal Producers that the buffer should no longer be full
		pthread_cond_signal(&condProducer);

		// Unlock mutex to free up buffer access
		pthread_mutex_unlock(&mutex1);
	}
}

// Producer thread function
void *pFunc(void *arg){

	// Keep thread alive indefinitely
	while(1){

		// Wait a random amount of time before producing
		sleep((getRandNum() % 5) + 3);

		// Lock mutex to gain exclusive access to the buffer
		pthread_mutex_lock(&mutex1);

		// While the buffer is full...
		while(bufferPointer >= BUFFERSIZE){

			// Print a message and wait on the producer condition variable
			printf("Buffer full, waiting\n");
			pthread_cond_wait(&condProducer, &mutex1);
		}

		// Generate data and add it to the buffer
		int wTime = (getRandNum() % 8) + 2;
		int bNum = getRandNum();
		buffer[bufferPointer].wait_time = wTime;
		buffer[bufferPointer].number = bNum;
		printf("Produced: Number %d, Wait Time %d\n", bNum, wTime);

		bufferPointer++;

		// Signal Consumers that the buffer should no longer be empty
		pthread_cond_signal(&condConsumer);

		// Unlock mutex to free up buffer access
		pthread_mutex_unlock(&mutex1);
	}
}

int main(int argc, char* argv[]){

	// Check if system supports rdrand
	checkSystem();
	
	int i; // Loop iterator
	int numThreads = 4; // Number of threads to create of each type
	pthread_t prod[numThreads]; // Producer array
	pthread_t cons[numThreads]; // Consumer Array

	// Seed Mersenne Twister if necessary
	if(x86system == 1)
	{
		unsigned long s = time(NULL);
		init_genrand(s);
	}

	// Generate threads
	for (i = 0; i < numThreads; i++) {
		pthread_create(&prod[i], NULL, pFunc, NULL);
		pthread_create(&cons[i], NULL, cFunc, NULL);
	}

	// Suspend main thread and wait to join finished threads
	for (i = 0; i < numThreads; i++) {
		pthread_join(prod[i], NULL);
		pthread_join(cons[i], NULL);
	}

	return 0;
}
