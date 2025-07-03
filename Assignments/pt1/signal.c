/*
Mason Lohnes
	The put and get functions use a mutex and condition to sleep
and then signal to eachother when to wake up.
*/

#include <stdio.h>
#include <pthread.h>

int theProduct;
int transferNumber;
int busySignal = 0;

pthread_mutex_t transferMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int produce(){
	return theProduct++;
}

void consume(int i){
	printf("%i\n", i);
}

void put(int i){
	pthread_mutex_lock(&transferMutex);
	while (busySignal){
		pthread_cond_wait(&cond, &transferMutex);
	}
	transferNumber = i;
	busySignal = 1;
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&transferMutex);
}

int get(){
	int i;
	pthread_mutex_lock(&transferMutex);
	while (!busySignal){
		pthread_cond_wait(&cond, &transferMutex);
	}
	i = transferNumber;
	busySignal = 0;
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&transferMutex);
	return i;
}

void *producer(void *arg){
	int i;
	while(1){
		i = produce();
		put(i);
	}
	return NULL;
}

void *consumer(void *arg){
	int i;
	while(1){
		i = get();
		consume(i);
	}
	return NULL;
}

int main() {
	pthread_t producerThread;
	pthread_t consumerThread;

	int producerErrorCheck = pthread_create(&producerThread, NULL, producer, NULL);
		if (producerErrorCheck) {
			fprintf(stderr, "Error - pthread_create() return code: %d\n", producerErrorCheck);
			return 1;

		}

	int consumerErrorCheck = pthread_create(&consumerThread, NULL, consumer, NULL);
		if (consumerErrorCheck) {
			fprintf(stderr, "Error - pthread_create() return code: %d\n", consumerErrorCheck);
			return 1;

		}
	pthread_join(producerThread, NULL);
	pthread_join(consumerThread, NULL);

	return 0;
}
