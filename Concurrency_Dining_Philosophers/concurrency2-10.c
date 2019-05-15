#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "mt19937ar.h"

#define bit_RDRND (1 << 30) //RDRND flag

int x86system;

struct Data{
	int position;
	char* name;
};

static pthread_mutex_t general = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t forks[5];
int forks_in_use[5] = {0};

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


void *pThread(void *arg){
	while(1){
		//Extract philosopher information
		struct Data *phil = (struct Data*)arg;

		//Philosopher name and position at table
		int position = (*phil).position;
		char *name = (*phil).name;
	
		//Number of forks held by philospher
		int holding = 0;

		//Philosopher thinks for a bit
		int thinking = (getRandNum() % 20) + 1;
		printf("%s thinking for %d seconds. \n", name, thinking);
		sleep(thinking);

		//Grab first fork (lowest numbered one) - Wait here if it's taken
		if(position != 4){	
			pthread_mutex_lock(&forks[position]);
			forks_in_use[position] = 1;
			holding++;
		}else{
			//If final philosopher at table, wrap fork values around
			pthread_mutex_lock(&forks[0]);
			forks_in_use[0] = 1;
			holding++;
		}

		//Grab second fork (higher of two options) - Wait if taken
		if(position != 4){
			pthread_mutex_lock(&forks[position + 1]);
			forks_in_use[position + 1] = 1;
			holding++;
		}else{
			//If final philosopher, adjust for wrap around
			pthread_mutex_lock(&forks[position]);
			forks_in_use[position] = 1;
			holding++;
		}

		//Eat if philosopher is holding two forks
		if(holding == 2){
			int eat = (getRandNum() % 8) + 2;
			printf("%s is holding two forks! Eating for %d\n", name, eat);
			sleep(eat);
			holding == 0;
		}

		//Releasing locks and returning forks
		pthread_mutex_unlock(&forks[position]);
		forks_in_use[position] = 0;
		if(position != 4){
			pthread_mutex_unlock(&forks[position + 1]);
			forks_in_use[position + 1] = 0;
		}else{
			pthread_mutex_unlock(&forks[0]);
			forks_in_use[0] = 0;
		}
		printf("%s has returned both forks.\n", name);

                //Display current status of forks 
		// Note: Other philosophers can grab forks before this prints.
                pthread_mutex_lock(&general);
                int i;
                printf("Status of forks: (0 means on table, 1 means in use). \n");
                for(i = 0; i < 5; i++){
			if(i != 4){
                        	printf("%d, ", forks_in_use[i]);
			}else{
				printf("%d ", forks_in_use[i]);
			}
                }
                printf("\n");
                pthread_mutex_unlock(&general);
		
	}
}

int main(int argc, char* argv[]){
	
	// Check if system supports rdrand
	checkSystem();

	int i;
	pthread_t phil[5];
	
	struct Data entity[5];

	entity[0].name = "Aristotle";
	entity[0].position = 0;
	entity[1].name = "Bacon";
	entity[1].position = 1;
	entity[2].name = "Comte";
	entity[2].position = 2;
	entity[3].name = "Democritus";
	entity[3].position = 3;
	entity[4].name = "Empdocles";
	entity[4].position = 4;

	//Seeding Mersenne Twister if Necessary
	if(x86system == 1){
		unsigned long s = time(NULL);
		init_genrand(s);
	}

	for(i = 0; i < 5; i++){
		pthread_mutex_init(&forks[i], NULL);
		pthread_create(&phil[i], NULL, pThread, (void*) &entity[i]);
	}
	
	for(i = 0; i < 5; i++){
		pthread_join(phil[i], NULL);
	}

	return 0;
}
