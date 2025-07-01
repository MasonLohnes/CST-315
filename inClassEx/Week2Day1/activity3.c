#include <stdio.h>
#include <pthread.h>

int counter = 0;
pthread_mutex_t counter_mutex;

void *print_message_function(void *ptr) {
	char *message;
	message = (char *) ptr;
	printf("%s \n", message);

	pthread_mutex_lock(&counter_mutex);
	counter++;
	printf("Counter is now at: %d\n", counter);
	pthread_mutex_unlock(&counter_mutex);

	return NULL;
}

#define NUM_THREADS 3

int main() {

	pthread_t threads[NUM_THREADS];
	const char *messages[NUM_THREADS] = {
		"Thread 1: Numero Uno",
		"Thread 2: Numeor Dos",
		"Thread 3: Numero Tres"
	};

	for (int i = 0; i < NUM_THREADS; i++) {
		int errorCheck = pthread_create(&threads[i], NULL, print_message_function, (void*)messages[i]);
		if (errorCheck) {
			fprintf(stderr, "Error - pthread_create() return code: %d\n", errorCheck);
			return 1;
		}
	}

	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	printf("Main thread done.\nThe final counter value was: %d\n", counter);
	return 0;
}
