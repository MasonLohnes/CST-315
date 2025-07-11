#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_READERS 5
#define NUM_WRITERS 2

// Monitor structure
typedef struct {
    int read_count;
    pthread_mutex_t mutex;
    pthread_cond_t read_phase;
    pthread_cond_t write_phase;
    int writer_waiting;
} Monitor;

Monitor monitor = {0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, 0};

void start_read() {
    pthread_mutex_lock(&monitor.mutex);
    while (monitor.writer_waiting > 0) {
        pthread_cond_wait(&monitor.read_phase, &monitor.mutex);
    }
    monitor.read_count++;
    pthread_mutex_unlock(&monitor.mutex);
}

void end_read() {
    pthread_mutex_lock(&monitor.mutex);
    monitor.read_count--;
    if (monitor.read_count == 0) {
        pthread_cond_signal(&monitor.write_phase);
    }
    pthread_mutex_unlock(&monitor.mutex);
}

void start_write() {
    pthread_mutex_lock(&monitor.mutex);
    monitor.writer_waiting++;
    while (monitor.read_count > 0) {
        pthread_cond_wait(&monitor.write_phase, &monitor.mutex);
    }
}

void end_write() {
    monitor.writer_waiting--;
    if (monitor.writer_waiting == 0) {
        pthread_cond_broadcast(&monitor.read_phase);
    } else {
        pthread_cond_signal(&monitor.write_phase);
    }
    pthread_mutex_unlock(&monitor.mutex);
}

void* reader(void* arg) {
    int id = *((int*)arg);
    while (1) {
        start_read();

        // Reading section
        printf("Reader %d is reading\n", id);
        sleep(rand() % 3);

        end_read();

        sleep(rand() % 3);
    }
    return NULL;
}

void* writer(void* arg) {
    int id = *((int*)arg);
    while (1) {
        start_write();

        // Writing section
        printf("Writer %d is writing\n", id);
        sleep(rand() % 3);

        end_write();

        sleep(rand() % 3);
    }
    return NULL;
}

int main() {
    pthread_t readers[NUM_READERS], writers[NUM_WRITERS];
    int reader_ids[NUM_READERS], writer_ids[NUM_WRITERS];

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

    pthread_mutex_destroy(&monitor.mutex);
    pthread_cond_destroy(&monitor.read_phase);
    pthread_cond_destroy(&monitor.write_phase);

    return 0;
}
