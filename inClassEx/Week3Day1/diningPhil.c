#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_PHILOSOPHERS 5

pthread_mutex_t forks[NUM_PHILOSOPHERS];
pthread_t philosophers[NUM_PHILOSOPHERS];
int foodLeft[NUM_PHILOSOPHERS];

void think(int philosopher) {
    printf("Philosopher %d is thinking.\n", philosopher);
    sleep(rand() % 3); // Random think time
}

void eat(int philosopher) {
    printf("Philosopher %d is eating.\n", philosopher);
    sleep(rand() % 3); // Random eat time
    foodLeft[philosopher]++;
}

void pick_up_forks(int philosopher) {
    int left = philosopher;
    int right = (philosopher + 1) % NUM_PHILOSOPHERS;

    // Ensure ordering to avoid deadlock
    if (philosopher % 2 == 0) {
        pthread_mutex_lock(&forks[left]);
        pthread_mutex_lock(&forks[right]);
    } else {
        pthread_mutex_lock(&forks[right]);
        pthread_mutex_lock(&forks[left]);
    }

    printf("Philosopher %d picked up forks %d and %d.\n", philosopher, left, right);
}

void put_down_forks(int philosopher) {
    int left = philosopher;
    int right = (philosopher + 1) % NUM_PHILOSOPHERS;

    pthread_mutex_unlock(&forks[left]);
    pthread_mutex_unlock(&forks[right]);

    printf("Philosopher %d put down forks %d and %d.\n", philosopher, left, right);
}



void* philosopher(void* num) {
    int philosopher = *(int*)num;
    while (foodLeft[philosopher] < 3) {
        think(philosopher);
        pick_up_forks(philosopher);
        eat(philosopher);
        put_down_forks(philosopher);
    }
    printf("Philosopher %d is full!\n", philosopher);
    return NULL;
}

int main() {
    int i;
    int philosopher_numbers[NUM_PHILOSOPHERS];

    for (i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_mutex_init(&forks[i], NULL);
    }

    for (i = 0; i < NUM_PHILOSOPHERS; i++) {
        foodLeft[i] = 0;
    }


    for (i = 0; i < NUM_PHILOSOPHERS; i++) {
        philosopher_numbers[i] = i;
        pthread_create(&philosophers[i], NULL, philosopher, &philosopher_numbers[i]);
    }

    for (i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_join(philosophers[i], NULL);
    }

    for (i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_mutex_destroy(&forks[i]);
    }

    printf("\nEveryone is done eating!\n\n");

    return 0;
}
