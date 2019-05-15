#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

#include "mt19937ar.h"

#define SEARCHERS 3
#define INSERTERS 3
#define DELETERS 2

#define bit_RDRND (1 << 30) // RDRND flag

int x86system;
int is_delete_active = 0;

//node for linked list
struct node{
   int data;
   struct node *next;
};

struct node *head = NULL;

pthread_cond_t condWait = PTHREAD_COND_INITIALIZER;

pthread_mutex_t genLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t iLock = PTHREAD_MUTEX_INITIALIZER;

//Check if system supports rdrand or not
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

//Check the system, use correct rand function
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


void printList(struct node *node){
	printf("printing LL\n");
	while(node != NULL){
		printf("%d\n", node->data);
		node = node->next;
   }
}

//inserts node to the end of a linked list
void insert(struct node **head, int data2add){
	struct node *node2add = (struct node*)malloc(sizeof(struct node));
	node2add->data = data2add;
	node2add->next = NULL;
	
	struct node *curr = *head;

	if(*head == NULL){
		*head = node2add;
	}
	else{
		while(curr->next != NULL){
			curr = curr->next;
		}
		curr->next = node2add;
	}
}

//finds a number in the list - only once though and not where in the list
void search(struct node **head, int data2find){
        //use curr to iterate
        struct node *curr = *head;

        if(*head == NULL){ //empty list case
                printf("could not find %d because list is empty\n", data2find);
        }
        else{              //look through non-empty list
                bool found = false;
                while(curr->next != NULL){  //find data from first node to second to last
                        if(curr->data == data2find){
                                found = true;
                                break;
                        }
                        else{
                                curr = curr->next;
                        }
                }
                if(curr->data == data2find){  //check if the last node has data2find
                        printf("found %d in the list \n", data2find);
                        found = true;
                        return;
                }
                if(found == false){  //if data2find is not found, report it
                        printf("%d is not in the list\n", data2find);
                }
        }

}

void delete(struct node **head, int data2remove){
        //use curr to iterate
        struct node *curr = *head;

        //use prev to keep track of the previous node
        struct node *prev;

        if(*head == NULL){ //empty list case
                printf("could not delete %d because list is empty\n", data2remove);
        }
        else{ //non-empty list case
                if(curr->data == data2remove){  //if the first element is the one you want to remove
                        *head = curr->next;
                        free(curr);  //actually delete it
                        return;
                }
                while(curr->next != NULL && curr->data != data2remove){
                        //do something
                        prev = curr;
                        curr = curr->next;
                }
                if(curr->data == data2remove){   //if the last node is the one we want to delete
                        //delete curr
                        prev->next = curr->next;
                        free(curr);  //actually delete it
                }
        }
}

void *iThread(){
	
	while(1){
		int random_value = getRandNum() % 9 + 1;
		int random_sleep = getRandNum() % 15 + 5;

		// Generic wait time to improve readability
		sleep(random_sleep);

		// Wait in the event that delete thread is active
		pthread_mutex_lock(&genLock);
		while(is_delete_active){
			printf("Delete is active - halting insert.\n");
			pthread_cond_wait(&condWait, &genLock);
		}
		pthread_mutex_unlock(&genLock);
		
		// Exclusive use by only one insert thread at a time
		pthread_mutex_lock(&iLock);
		pthread_mutex_lock(&genLock);
	
		printf("Inserting %d into list.\n", random_value);	
		insert(&head, random_value);
		//insert(&head, 6789);
		

		//printList(head);

		pthread_mutex_unlock(&iLock);
		pthread_mutex_unlock(&genLock);
	}
}
void *sThread(){
	
	while(1){
		int random_value = getRandNum() % 9 + 1;
		int random_sleep = getRandNum() % 15 + 5;	

		// Generic wait time to improve readability
		sleep(random_sleep);

		// Wait in the event that delete thread is active
		pthread_mutex_lock(&genLock);
		while(is_delete_active){
			printf("Delete is active - halting search.\n");
			pthread_cond_wait(&condWait, &genLock);
		}
		pthread_mutex_unlock(&genLock);

		// Thread is now cleared to safely search
		printf("Searching for %d in list.\n", random_value);
		search(&head, random_value);
		
		//printList(head);

	}
}
void *dThread(){
	
	while(1){
		int random_value = getRandNum() % 9 + 1;
		int random_sleep = getRandNum() % 15 + 5;
		
		// Generic wait time to improve readability
		sleep(random_sleep);

		// Lock all other processes
		pthread_mutex_lock(&genLock);
		
		// Alert other processes that a deletion lockout is in effect
		is_delete_active = 1;

		// Give time to let other active processes complete
		sleep(2);

		//printf("\n !! DELETE STARTING !! \n\n");
		//printList(head);

		printf("Deleting %d from list.\n", random_value);
		delete(&head, random_value);
		
		//printf(" !! DELETE ENDING !! \n\n");	
		printList(head);
		is_delete_active = 0;
		pthread_cond_broadcast(&condWait);
		pthread_mutex_unlock(&genLock);
	}
}


int main(){

	checkSystem();
	
	int i;
	pthread_t insertThread[INSERTERS];
	pthread_t searchThread[SEARCHERS];
	pthread_t deleteThread[DELETERS];
 
	if(x86system == 1){
		unsigned long s = time(NULL);
		init_genrand(s);
	}
  
	for(i = 0; i < INSERTERS; i++){
		pthread_create(&insertThread[i], NULL, iThread, NULL);
   	}

	for(i = 0; i < SEARCHERS; i++){
		pthread_create(&searchThread[i], NULL, sThread, NULL);
	}

	for(i = 0; i < DELETERS; i++){
		pthread_create(&deleteThread[i], NULL, dThread, NULL);
	}
     
   	for(i = 0; i < INSERTERS; i++){
		pthread_join(insertThread[i], NULL);
	}

	for(i = 0; i < SEARCHERS; i++){
		pthread_join(searchThread[i], NULL);
	}

	for(i = 0; i < DELETERS; i++){
		pthread_join(deleteThread[i], NULL);
	}
  

}
