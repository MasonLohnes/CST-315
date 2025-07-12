// Author: Mason Lohnes
// Description: Uses monitor-style synchronization (mutex + condition variable)

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_USERS 5

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int printer_busy = 0;

void* print_job(void* arg) {
	int id = *(int*)arg;

	printf("User %d is waiting to print.\n", id);

	pthread_mutex_lock(&lock);
	while (printer_busy) {
		pthread_cond_wait(&cond, &lock);
	}

	printer_busy = 1;
	pthread_mutex_unlock(&lock);

	printf("User %d is printing...\n", id);
	sleep(rand() % 3 + 1);
	printf("User %d finished printing.\n", id);

	pthread_mutex_lock(&lock);
	printer_busy = 0;
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&lock);
}

int main() {
	pthread_t users[NUM_USERS];
	int ids[NUM_USERS];

	for (int i = 0; i < NUM_USERS; i++) {
		ids[i] = i + 1;
		pthread_create(&users[i], NULL, print_job, &ids[i]);
	}

	for (int i = 0; i < NUM_USERS; i++) {
		pthread_join(users[i], NULL);
	}

	return 0;
}
