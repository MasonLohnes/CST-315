#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>

#define MAX_CHILDREN 10
#define TIME_SLICE 2
#define NUM_THREADS 5 // FIXED: You had #define NUM_THREADS without a value

typedef enum { NEW, READY, RUNNING, WAITING, TERMINATED } State;

typedef struct PCB {
    int pid;
    State state;
    int priority;
    int cpu_time_used;
    int time_limit;
    int io_blocked_time;
    struct PCB *parent;
    struct PCB *children[MAX_CHILDREN];
    int child_count;
    struct PCB *next; // for queueing
} PCB;

typedef struct Queue {
    PCB *head;
    PCB *tail;
    pthread_mutex_t lock;
} Queue; // FIXED: Missing semicolon

void enqueue(Queue *q, PCB *process) {
    pthread_mutex_lock(&q->lock);
    process->next = NULL;

    if (!q->head) { // FIXED: lowercase `if`
        q->head = q->tail = process;
    } else {
        q->tail->next = process;
        q->tail = process;
    }

    pthread_mutex_unlock(&q->lock); // FIXED: Added argument to unlock
}

PCB* dequeue(Queue *q) {
    pthread_mutex_lock(&q->lock);
    PCB* temp = q->head;

    if (q->head) { // FIXED: lowercase `if`
        q->head = q->head->next;
        if (!q->head) { // FIXED: lowercase `if`, and logic
            q->tail = NULL;
        }
    }

    pthread_mutex_unlock(&q->lock);
    return temp;
}

PCB* create_process(int pid, int priority, int time_limit) {
    PCB* p = malloc(sizeof(PCB));
    p->pid = pid;
    p->state = NEW;
    p->priority = priority;
    p->cpu_time_used = 0;
    p->time_limit = time_limit;
    p->io_blocked_time = 0;
    p->parent = NULL;
    p->child_count = 0;
    p->next = NULL;
    return p;
}

void scheduler_tick(Queue* readyQueue, Queue* waitQueue) {
    PCB* running = dequeue(readyQueue);
    if (!running) {
        printf("[Tick] No process is ready.\n");
        return;
    }

    running->state = RUNNING;
    printf("[Tick] Running process %d\n", running->pid);
    sleep(1); // simulate work

    running->cpu_time_used += TIME_SLICE;
    if (running->cpu_time_used >= running->time_limit) {
        running->state = TERMINATED;
        printf("[Exit] Process %d finished.\n", running->pid);
        free(running);
    } else if (rand() % 4 == 0) {
        running->state = WAITING;
        running->io_blocked_time = 3;
        enqueue(waitQueue, running);
        printf("[IO] Process %d moved to the waiting queue.\n", running->pid);
    } else {
        running->state = READY;
        enqueue(readyQueue, running);
    }
}

void update_wait_queue(Queue* waitQueue, Queue* readyQueue) {
    PCB* prev = NULL;
    PCB* curr;

    pthread_mutex_lock(&waitQueue->lock);
    curr = waitQueue->head;

    while (curr) {
        curr->io_blocked_time--;
        if (curr->io_blocked_time <= 0) {
            PCB* done = curr;

            // Remove from waitQueue
            if (prev) {
                prev->next = curr->next;
            } else {
                waitQueue->head = curr->next;
            }
            if (waitQueue->tail == curr) {
                waitQueue->tail = prev;
            }

            curr = curr->next; // Advance before enqueuing

            done->state = READY;
            enqueue(readyQueue, done);
            printf("[IO Done] Process %d moved to ready queue.\n", done->pid);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }

    pthread_mutex_unlock(&waitQueue->lock);
}

int main() {
    Queue waitQueue;
    waitQueue.head = NULL;
    waitQueue.tail = NULL;
    pthread_mutex_init(&waitQueue.lock, NULL); // FIXED: missing semicolon

    Queue readyQueue;
    readyQueue.head = NULL;
    readyQueue.tail = NULL;
    pthread_mutex_init(&readyQueue.lock, NULL); // FIXED: missing semicolon

    pthread_t threads[NUM_THREADS]; // Currently unused

    for (int i = 0; i < NUM_THREADS; i++) {
        PCB* p = create_process(i, rand() % 3, 6 + rand() % 5);
        p->state = READY;
        enqueue(&readyQueue, p);
    }

    for (int i = 0; i < 15; i++) {
        printf("\n== TICK %d ==\n", i + 1);
        scheduler_tick(&readyQueue, &waitQueue);
        update_wait_queue(&waitQueue, &readyQueue);
    }

    pthread_mutex_destroy(&readyQueue.lock);
    pthread_mutex_destroy(&waitQueue.lock);

    return 0;
}
