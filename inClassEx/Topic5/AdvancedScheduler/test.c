#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#define MAX_CHILDREN 10
#define TIME_SLICE 2
#define IO_WAIT_TIME 3

// Process states
typedef enum { NEW, READY, RUNNING, WAITING, TERMINATED } State;

// Process Control Block (PCB)
typedef struct PCB {
    int pid;
    State state;
    int priority;
    int cpu_time_used;
    int time_limit;
    int arrivalTime;
    int io_blocked_time;
    struct PCB* parent;
    struct PCB* children[MAX_CHILDREN];
    int child_count;
    struct PCB* next;
} PCB;

// I/O request structure
typedef struct io_request {
    int process_id;
    int io_type; // 0 = disk, 1 = network, etc.
    void (*completion_handler)(int);
} io_request;

// Queue structure
typedef struct Queue {
    PCB* head;
    PCB* tail;
    pthread_mutex_t lock;
} Queue;

// Queue operations
void enqueue(Queue* q, PCB* process) {
    pthread_mutex_lock(&q->lock);
    process->next = NULL;
    if (!q->head) {
        q->head = q->tail = process;
    } else {
        q->tail->next = process;
        q->tail = process;
    }
    pthread_mutex_unlock(&q->lock);
}

PCB* dequeue(Queue* q) {
    pthread_mutex_lock(&q->lock);
    PCB* temp = q->head;
    if (q->head) {
        q->head = q->head->next;
        if (!q->head) q->tail = NULL;
    }
    pthread_mutex_unlock(&q->lock);
    return temp;
}

// Create a new PCB
PCB* create_process(int pid, int priority, int burstTime, int arrivalTime) {
    PCB* p = malloc(sizeof(PCB));
    p->pid = pid;
    p->state = NEW;
    p->priority = priority;
    p->cpu_time_used = 0;
    p->time_limit = burstTime;
    p->arrivalTime = arrivalTime;
    p->io_blocked_time = 0;
    p->parent = NULL;
    p->child_count = 0;
    p->next = NULL;
    return p;
}

// Function to handle I/O request
typedef struct {
    Queue* waitQueue;
    Queue* readyQueue;
} IOQueues;

IOQueues io_queues;

void handle_io_request(PCB* proc, io_request* req) {
    printf("[IO Request] Process %d requested I/O type %d\n", req->process_id, req->io_type);
    proc->state = WAITING;
    proc->io_blocked_time = IO_WAIT_TIME;
    enqueue(io_queues.waitQueue, proc);
}

void simulate_io_completion(int process_id) {
    printf("[IO Completion] Process %d I/O completed\n", process_id);
}

// Simulate scheduler tick
void scheduler_tick(Queue* readyQueue, Queue* waitQueue) {
    PCB* running = NULL;  // <-- Declare it here!

    if (!running) {
        running = dequeue(readyQueue);
        if (!running) {
            printf("[Tick] No process is ready.\n");
            return;
        }
    }

    running->state = RUNNING;
    printf("[Tick] Running process %d\n", running->pid);
    sleep(1);

    running->cpu_time_used += TIME_SLICE;
    if (running->cpu_time_used >= running->time_limit) {
        running->state = TERMINATED;
        printf("[Exit] Process %d finished.\n", running->pid);
        free(running);
    } else if (rand() % 4 == 0) {
        io_request req = {running->pid, rand() % 2, simulate_io_completion};
        handle_io_request(running, &req);
    } else {
        running->state = READY;
        enqueue(readyQueue, running);
    }
}



// Simulate I/O wait queue
void update_wait_queue(Queue* waitQueue, Queue* readyQueue) {
    PCB* prev = NULL;
    PCB* curr;

    pthread_mutex_lock(&waitQueue->lock);
    curr = waitQueue->head;
    while (curr) {
        curr->io_blocked_time--;
        if (curr->io_blocked_time <= 0) {
            PCB* done = curr;
            if (prev) prev->next = curr->next;
            else waitQueue->head = curr->next;
            if (waitQueue->tail == curr) waitQueue->tail = prev;

            curr = curr->next;
            done->state = READY;
            simulate_io_completion(done->pid);
            enqueue(readyQueue, done);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
    pthread_mutex_unlock(&waitQueue->lock);
}

int main() {
    Queue readyQueue = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};
    Queue waitQueue = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};
    io_queues.readyQueue = &readyQueue;
    io_queues.waitQueue = &waitQueue;

    FILE *f = fopen("process_batch.txt", "r");
    if (!f) {
        perror("Batch file not found");
        return 1;
    }

    int n;
    fscanf(f, "%d", &n);
    for (int i = 0; i < n; i++) {
        int id, at, bt, pr;
        fscanf(f, "%d %d %d %d", &id, &at, &bt, &pr);
        PCB *p = create_process(id, pr, bt, at);
        p->state = READY;
        enqueue(&readyQueue, p);
    }
    fclose(f);

    for (int t = 0; t < 200; t++) {
        printf("\n== Tick %d ==\n", t + 1);
        scheduler_tick(&readyQueue, &waitQueue);
        update_wait_queue(&waitQueue, &readyQueue);
    }

    return 0;
}


