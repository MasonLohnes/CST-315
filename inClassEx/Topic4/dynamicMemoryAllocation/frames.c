#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

pthread_mutex_t memory_lock = PTHREAD_MUTEX_INITIALIZER;

#define PAGE_SIZE 4096
#define NUM_PAGES 100
#define NUM_FRAMES 50
#define MAX_PROCESSES 10

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
} Process;

typedef struct {
	int frame_number;
	int occupied;
	int process_id;
	int page_number;
} Frame;

typedef struct {
	int process_id;
	int page_number;
} Page;

Frame frames[NUM_FRAMES];
Process processes[MAX_PROCESSES];
int process_count = 0;

// FIFO queue for page replacement
Page page_queue[NUM_FRAMES];
int queue_front = 0, queue_rear = 0, queue_size = 0;

void enqueue_page(int pid, int page_num) {
	page_queue[queue_rear].process_id = pid;
	page_queue[queue_rear].page_number = page_num;
	queue_rear = (queue_rear + 1) % NUM_FRAMES;
	queue_size++;
}

Page dequeue_page() {
	Page p = page_queue[queue_front];
	queue_front = (queue_front + 1) % NUM_FRAMES;
	queue_size--;
	return p;
}

void initialize_frames() {
	for (int i = 0; i < NUM_FRAMES; i++) {
		frames[i].frame_number = i;
		frames[i].occupied = 0;
		frames[i].process_id = -1;
		frames[i].page_number = -1;
	}
}

void initialize_page_table(Process *process) {
	for (int i = 0; i < NUM_PAGES; i++) {
		process->page_table[i].frame_number = -1;
		process->page_table[i].valid = 0;
		process->page_table[i].modified = 0;
		process->page_table[i].read_permission = 1;
		process->page_table[i].write_permission = 1;
	}
}

int create_process() {
	if (process_count >= MAX_PROCESSES) {
		fprintf(stderr, "Maximum process limit reached\n");
		return -1;
	}
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

	printf("Evicting page %d of process %d from frame %d\n",
		victim.page_number, victim.process_id, victim_frame);

	victim_process->page_table[victim.page_number].valid = 0;
	victim_process->page_table[victim.page_number].frame_number = -1;

	enqueue_page(requesting_process->process_id, page_number);
	return victim_frame;
}

void load_page(Process *process, int page_number) {
	int frame_number = allocate_frame_fifo(process, page_number);
	process->page_table[page_number].frame_number = frame_number;
	process->page_table[page_number].valid = 1;

	frames[frame_number].occupied = 1;
	frames[frame_number].process_id = process->process_id;
	frames[frame_number].page_number = page_number;

	printf("Loaded page %d into frame %d for process %d\n",
		page_number, frame_number, process->process_id);
}

void access_memory(Process *process, int page_number, int offset, char mode) {
	if (!process->page_table[page_number].valid) {
		load_page(process, page_number);
	}
	if ((mode == 'r' && !process->page_table[page_number].read_permission) ||
		(mode == 'w' && !process->page_table[page_number].write_permission)) {
		printf("Access violation on process %d, page %d, offset %d, mode %c\n",
			process->process_id, page_number, offset, mode);
		return;
	}
	int frame_number = process->page_table[page_number].frame_number;
	printf("Accessed memory at frame %d, offset %d for process %d, mode %c\n",
		frame_number, offset, process->process_id, mode);
}

void access_memory_threadsafe(Process *process, int page_number, int offset, char mode) {
	pthread_mutex_lock(&memory_lock);
	access_memory(process, page_number, offset, mode);
	pthread_mutex_unlock(&memory_lock);
}

int realloc_pages(Process *process, int new_size) {
	for (int i = 0; i < NUM_PAGES && new_size > 0; i++) {
		if (!process->page_table[i].valid) {
			load_page(process, i);
			new_size--;
		}
	}
	return 0;
}

void free_frames(Process *process) {
	for (int i = 0; i < NUM_PAGES; i++) {
		if (process->page_table[i].valid) {
			int frame_number = process->page_table[i].frame_number;
			frames[frame_number].occupied = 0;
			frames[frame_number].process_id = -1;
			frames[frame_number].page_number = -1;
			process->page_table[i].valid = 0;
			printf("Freed frame %d from page %d for process %d\n",
				frame_number, i, process->process_id);
		}
	}
}

void print_memory_state() {
	printf("\n=== Memory State ===\n");
	for (int i = 0; i < NUM_FRAMES; i++) {
		if (frames[i].occupied) {
			printf("Frame %d: Process %d, Page %d\n",
				i, frames[i].process_id, frames[i].page_number);
		} else {
			printf("Frame %d: Free\n", i);
		}
	}
	printf("====================\n");
}

void* process_thread(void* arg) {
	Process* process = (Process*) arg;

	for (int i = 0; i < 20; i++) {
		int page = rand() % NUM_PAGES;
		char mode = (rand() % 2) ? 'r' : 'w';
		int offset = rand() % PAGE_SIZE;

		access_memory_threadsafe(process, page, offset, mode);
		sleep(rand() % 3); // simulate delay between memory accesses
	}

	pthread_mutex_lock(&memory_lock);
	free_frames(process);
	pthread_mutex_unlock(&memory_lock);

	return NULL;
}


int main() {
	srand(time(NULL));
	initialize_frames();

	int p1 = create_process();
	int p2 = create_process();
	int p3 = create_process();
	int p4 = create_process();

	pthread_t t1, t2, t3, t4;

	if (p1 != -1 && p2 != -1 && p3 != -1 && p4 != -1) {
		pthread_create(&t1, NULL, process_thread, (void*)&processes[p1 - 1]);
		pthread_create(&t2, NULL, process_thread, (void*)&processes[p2 - 1]);
		pthread_create(&t3, NULL, process_thread, (void*)&processes[p3 - 1]);
		pthread_create(&t4, NULL, process_thread, (void*)&processes[p4 - 1]);

		pthread_join(t1, NULL);
		pthread_join(t2, NULL);
		pthread_join(t3, NULL);
		pthread_join(t4, NULL);
	}

	print_memory_state();
	return 0;
}

