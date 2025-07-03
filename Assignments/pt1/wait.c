/*
Mason Lohnes
	Sets a signal int as 0 or 1 based on whether the threads should use put or get.
Then wait 1 second each time the signal says busy.
*/

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

int theProduct;
int transferNumber;
int busySignal = 0;

int produce(){
	return theProduct++;
}

void consume(int i){
	printf("%i\n", i);
}

void put(int i){
	while (busySignal){
		sleep(1);
	}
	transferNumber = i;
	busySignal = 1;
}

int get(){
	int i;
	while (!busySignal){
		sleep(1);
	}
	i = transferNumber;
	busySignal = 0;
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
