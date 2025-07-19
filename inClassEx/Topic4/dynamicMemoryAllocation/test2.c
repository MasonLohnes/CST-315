#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 100
#define NUM_FRAMES 50
#define MAX_PROCESSES 10
#define TLB_SIZE 8
#define SWAP_SIZE 200
#define PFF_THRESHOLD 5

pthread_mutex_t memory_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int frame_number;
    int valid;
    int modified;
    int read_permission;
    int write_permission;
} PageTableEntry;

typedef struct {
    int process_id;
    PageTableEntry page_table[NUM_PAGES];
    int working_set_window;
    int recent_faults;
} Process;

typedef struct {
    int frame_number;
    int occupied;
    int process_id;
    int page_number;
} Frame;

typedef struct {
    int page_number;
    int frame_number;
    int valid;
    int use_counter;
} TLBEntry;

typedef struct {
    int process_id;
    int page_number;
} Page;

typedef struct {
    int process_id;
    int page_number;
    int in_use;
} SwapSlot;

Frame frames[NUM_FRAMES];
Process processes[MAX_PROCESSES];
TLBEntry tlb[TLB_SIZE];
SwapSlot swap[SWAP_SIZE];
Page page_queue[NUM_FRAMES];
int process_count = 0;
int queue_front = 0, queue_rear = 0, queue_size = 0;
int tlb_hits = 0, tlb_misses = 0, tlb_next = 0;

void enqueue_page(int pid, int page_num) {
    page_queue[queue_rear] = (Page){pid, page_num};
    queue_rear = (queue_rear + 1) % NUM_FRAMES;
    queue_size++;
}

Page dequeue_page() {
    Page p = page_queue[queue_front];
    queue_front = (queue_front + 1) % NUM_FRAMES;
    queue_size--;
    return p;
}

void simulate_disk_io() {
    sleep(1);
}

void initialize_frames() {
    for (int i = 0; i < NUM_FRAMES; i++) {
        frames[i] = (Frame){i, 0, -1, -1};
    }
    for (int i = 0; i < SWAP_SIZE; i++) {
        swap[i].in_use = 0;
    }
}

void initialize_page_table(Process *process) {
    for (int i = 0; i < NUM_PAGES; i++) {
        process->page_table[i] = (PageTableEntry){-1, 0, 0, 1, 1};
    }
    process->working_set_window = 10;
    process->recent_faults = 0;
}

void initialize_tlb() {
    for (int i = 0; i < TLB_SIZE; i++) tlb[i].valid = 0;
}

int create_process() {
    if (process_count >= MAX_PROCESSES) return -1;
    Process *process = &processes[process_count];
    process->process_id = process_count + 1;
    initialize_page_table(process);
    process_count++;
    return process->process_id;
}

int allocate_frame() {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (!frames[i].occupied) {
            frames[i].occupied = 1;
            return i;
        }
    }
    return -1;
}

int allocate_frame_fifo(Process *requesting_process, int page_number) {
    int frame = allocate_frame();
    if (frame != -1) {
        enqueue_page(requesting_process->process_id, page_number);
        return frame;
    }

    Page victim = dequeue_page();
    Process *victim_process = &processes[victim.process_id - 1];
    int victim_frame = victim_process->page_table[victim.page_number].frame_number;

    printf("Evicting page %d of process %d from frame %d\n", victim.page_number, victim.process_id, victim_frame);

    for (int i = 0; i < SWAP_SIZE; i++) {
        if (!swap[i].in_use) {
            swap[i] = (SwapSlot){victim.process_id, victim.page_number, 1};
            break;
        }
    }

    victim_process->page_table[victim.page_number].valid = 0;
    victim_process->page_table[victim.page_number].frame_number = -1;
    enqueue_page(requesting_process->process_id, page_number);
    return victim_frame;
}

int tlb_lookup(int page_number, int *frame_number) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].page_number == page_number) {
            *frame_number = tlb[i].frame_number;
            tlb[i].use_counter = time(NULL);
            tlb_hits++;
            return 1;
        }
    }
    tlb_misses++;
    return 0;
}

void tlb_add_entry(int page_number, int frame_number) {
    tlb[tlb_next] = (TLBEntry){page_number, frame_number, 1, time(NULL)};
    tlb_next = (tlb_next + 1) % TLB_SIZE;
}

void load_page(Process *process, int page_number, int is_hard_fault) {
    if (is_hard_fault) simulate_disk_io();
    int frame_number = allocate_frame_fifo(process, page_number);
    process->page_table[page_number].frame_number = frame_number;
    process->page_table[page_number].valid = 1;
    frames[frame_number] = (Frame){frame_number, 1, process->process_id, page_number};
    printf("[LOAD] %s Page Fault: Loaded page %d into frame %d for process %d\n",
        is_hard_fault ? "Hard" : "Soft", page_number, frame_number, process->process_id);
}

void access_memory(Process *process, int page_number, int offset, char mode) {
    int frame_number;

    if (!tlb_lookup(page_number, &frame_number)) {
        if (!process->page_table[page_number].valid) {
            process->recent_faults++;
            int is_known = 0;
            for (int i = 0; i < SWAP_SIZE; i++) {
                if (swap[i].in_use && swap[i].process_id == process->process_id && swap[i].page_number == page_number) {
                    is_known = 1;
                    swap[i].in_use = 0;
                    break;
                }
            }
            load_page(process, page_number, !is_known);
        }
        frame_number = process->page_table[page_number].frame_number;
        tlb_add_entry(page_number, frame_number);
    }

    if ((mode == 'r' && !process->page_table[page_number].read_permission) ||
        (mode == 'w' && !process->page_table[page_number].write_permission)) {
        printf("Access violation on process %d, page %d, offset %d, mode %c\n",
            process->process_id, page_number, offset, mode);
        return;
    }

    printf("Accessed memory at frame %d, offset %d for process %d, mode %c\n",
           frame_number, offset, process->process_id, mode);
}

void access_memory_threadsafe(Process *process, int page_number, int offset, char mode) {
    pthread_mutex_lock(&memory_lock);
    access_memory(process, page_number, offset, mode);
    if (process->recent_faults > PFF_THRESHOLD) {
        process->working_set_window++;
        process->recent_faults = 0;
        printf("PFF control: Increased working set window for process %d\n", process->process_id);
    }
    pthread_mutex_unlock(&memory_lock);
}

void free_frames(Process *process) {
    for (int i = 0; i < NUM_PAGES; i++) {
        if (process->page_table[i].valid) {
            int frame_number = process->page_table[i].frame_number;
            frames[frame_number] = (Frame){frame_number, 0, -1, -1};
            process->page_table[i].valid = 0;
            printf("Freed frame %d from page %d for process %d\n", frame_number, i, process->process_id);
        }
    }
}

void* process_thread(void* arg) {
    Process* process = (Process*) arg;
    for (int i = 0; i < 20; i++) {
        int page = rand() % NUM_PAGES;
        char mode = (rand() % 2) ? 'r' : 'w';
        int offset = rand() % PAGE_SIZE;
        access_memory_threadsafe(process, page, offset, mode);
        sleep(rand() % 3);
    }
    pthread_mutex_lock(&memory_lock);
    free_frames(process);
    pthread_mutex_unlock(&memory_lock);
    return NULL;
}

void print_memory_state() {
    printf("\n=== Memory State ===\n");
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frames[i].occupied) {
            printf("Frame %d: Process %d, Page %d\n", i, frames[i].process_id, frames[i].page_number);
        } else {
            printf("Frame %d: Free\n", i);
        }
    }
    printf("====================\n");
}

void print_tlb_state() {
    printf("\n=== TLB Contents ===\n");
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid) {
            printf("TLB[%d]: Page %d -> Frame %d (use=%d)\n",
                i, tlb[i].page_number, tlb[i].frame_number, tlb[i].use_counter);
        }
    }
    printf("TLB Hits: %d | Misses: %d\n", tlb_hits, tlb_misses);
    printf("====================\n");
}

int main() {
    srand(time(NULL));
    initialize_frames();
    initialize_tlb();

    int p1 = create_process();
    int p2 = create_process();
    int p3 = create_process();
    int p4 = create_process();

    pthread_t t1, t2, t3, t4;

    if (p1 != -1 && p2 != -1 && p3 != -1 && p4 != -1) {
        pthread_create(&t1, NULL, process_thread, &processes[p1 - 1]);
        pthread_create(&t2, NULL, process_thread, &processes[p2 - 1]);
        pthread_create(&t3, NULL, process_thread, &processes[p3 - 1]);
        pthread_create(&t4, NULL, process_thread, &processes[p4 - 1]);

        pthread_join(t1, NULL);
        pthread_join(t2, NULL);
        pthread_join(t3, NULL);
        pthread_join(t4, NULL);
    }

    print_memory_state();
    print_tlb_state();
    return 0;
}

