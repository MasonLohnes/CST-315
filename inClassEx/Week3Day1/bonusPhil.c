#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define NUM_PHILOSOPHERS 5
#define NUM_FORKS 4
#define NUM_KNIVES 2

pthread_mutex_t forks[NUM_FORKS];               // 4 forks (no fork between 4 and 0)
pthread_mutex_t knife_mutex;
pthread_cond_t knife_cond;

int knives_available = NUM_KNIVES;

pthread_t philosophers[NUM_PHILOSOPHERS];
int foodLeft[NUM_PHILOSOPHERS];

// Maps each philosopher to their ONE allowed fork
// Note: Fork 3 is shared between 3 and 4
int philosopher_to_fork[NUM_PHILOSOPHERS] = {0, 1, 2, 3, 3};

void think(int philosopher) {
    printf("Philosopher %d is thinking.\n", philosopher);
    sleep(rand() % 3);
}

void eat(int philosopher) {
    printf("Philosopher %d is eating.\n", philosopher);
    sleep(rand() % 3);
    foodLeft[philosopher]++;
}

void acquire_knife() {
    pthread_mutex_lock(&knife_mutex);
    while (knives_available == 0) {
        pthread_cond_wait(&knife_cond, &knife_mutex);
    }
    knives_available--;
    pthread_mutex_unlock(&knife_mutex);
}

void release_knife() {
    pthread_mutex_lock(&knife_mutex);
    knives_available++;
    pthread_cond_signal(&knife_cond);
    pthread_mutex_unlock(&knife_mutex);
}

void* philosopher(void* num) {
    int id = *(int*)num;
    int fork_id = philosopher_to_fork[id];

    while (foodLeft[id] < 3) {
        think(id);

        // Step 1: Acquire a knife
        acquire_knife();

        // Step 2: Acquire fork assigned to this philosopher
        pthread_mutex_lock(&forks[fork_id]);

        // Step 3: Eat
        printf("Philosopher %d picked up fork %d and a knife.\n", id, fork_id);
        eat(id);
        printf("Philosopher %d put down fork %d and knife.\n", id, fork_id);

        // Step 4: Release fork and knife
        pthread_mutex_unlock(&forks[fork_id]);
        release_knife();
    }

    printf("Philosopher %d is full!\n", id);
    return NULL;
}

int main() {
    srand(time(NULL));
    int philosopher_numbers[NUM_PHILOSOPHERS];

    // Initialize resources
    pthread_mutex_init(&knife_mutex, NULL);
    pthread_cond_init(&knife_cond, NULL);
    for (int i = 0; i < NUM_FORKS; i++) {
        pthread_mutex_init(&forks[i], NULL);
    }

    // Start philosopher threads
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        foodLeft[i] = 0;
        philosopher_numbers[i] = i;
        pthread_create(&philosophers[i], NULL, philosopher, &philosopher_numbers[i]);
    }

    // Wait for all philosophers to finish
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_join(philosophers[i], NULL);
    }

    // Cleanup
    for (int i = 0; i < NUM_FORKS; i++) {
        pthread_mutex_destroy(&forks[i]);
    }
    pthread_mutex_destroy(&knife_mutex);
    pthread_cond_destroy(&knife_cond);

    printf("\nEveryone is done eating!\n\n");
    return 0;
}
