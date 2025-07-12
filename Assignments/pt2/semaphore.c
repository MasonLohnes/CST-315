// Author: Mason Lohnes
// Description: Uses semaphores to ensure exclusive access to a printer

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define NUM_USERS 5

sem_t printer;

void* print_job(void* arg) {
	int id = *(int*)arg;
	printf("User %d is waiting to print...\n", id);
	sem_wait(&printer);
	printf("User %d is printing...\n", id);
	sleep(rand() % 3 + 1);
	printf("User %d finished printing.\n", id);
	sem_post(&printer);
	return NULL;
}

int main() {
	pthread_t users[NUM_USERS];
	int ids[NUM_USERS];

	sem_init(&printer, 0, 1); // Binary semaphore

	for (int i = 0; i < NUM_USERS; i++) {
		ids[i] = i + 1;
		pthread_create(&users[i], NULL, print_job, &ids[i]);
	}

	for (int i = 0; i < NUM_USERS; i++) {
		pthread_join(users[i], NULL);
	}

	sem_destroy(&printer);
	return 0;
}
