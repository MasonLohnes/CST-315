#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define NUM_READERS 5
#define NUM_WRITERS 2

sem_t resource; // Semaphore for managing access to the resource
sem_t rmutex;   // Semaphore for protecting the read_count
int read_count = 0; // Number of readers currently accessing the resource

void* reader(void* arg) {
    int id = *((int*)arg);
    while (1) {
        sem_wait(&rmutex);
        read_count++;
        if (read_count == 1) {
            sem_wait(&resource); // First reader locks the resource
        }
        sem_post(&rmutex);

        // Reading section
        printf("Reader %d is reading\n", id);
        sleep(rand() % 3);

        sem_wait(&rmutex);
        read_count--;
        if (read_count == 0) {
            sem_post(&resource); // Last reader unlocks the resource
        }
        sem_post(&rmutex);
        
        sleep(rand() % 3);
    }
    return NULL;
}

void* writer(void* arg) {
    int id = *((int*)arg);
    while (1) {
        sem_wait(&resource); // Writer locks the resource

        // Writing section
        printf("Writer %d is writing\n", id);
        sleep(rand() % 3);

        sem_post(&resource); // Writer unlocks the resource

        sleep(rand() % 3);
    }
    return NULL;
}

int main() {
    pthread_t readers[NUM_READERS], writers[NUM_WRITERS];
    int reader_ids[NUM_READERS], writer_ids[NUM_WRITERS];

    sem_init(&resource, 0, 1);
    sem_init(&rmutex, 0, 1);

    for (int i = 0; i < NUM_READERS; i++) {
        reader_ids[i] = i + 1;
        pthread_create(&readers[i], NULL, reader, &reader_ids[i]);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_ids[i] = i + 1;
        pthread_create(&writers[i], NULL, writer, &writer_ids[i]);
    }

    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(writers[i], NULL);
    }

    sem_destroy(&resource);
    sem_destroy(&rmutex);

    return 0;
}
