#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 100

typedef struct {
    int page_number;
    int valid;
    void *frame_address;
} PageTableEntry;

typedef struct {
    int process_id;
    int memory_size;
    int num_pages;
    int *page_table_entries;  // Indices into global page_table[]
} Process;

PageTableEntry page_table[NUM_PAGES];
void *memory[NUM_PAGES];

void load_page(int page_number) {
    if (page_number < 0 || page_number >= NUM_PAGES) {
        fprintf(stderr, "Invalid page number %d\n", page_number);
        exit(1);
    }

    if (!page_table[page_number].valid) {
        void *frame = malloc(PAGE_SIZE);
        if (frame == NULL) {
            fprintf(stderr, "Memory allocation failed for page %d\n", page_number);
            exit(1);
        }
        page_table[page_number].valid = 1;
        page_table[page_number].frame_address = frame;
        memory[page_number] = frame;

        printf("Page fault: Loaded page %d into memory.\n", page_number);
    }
}

void simulate_operations(Process *process) {
    for (int i = 0; i < process->num_pages; i++) {
        int page_index = process->page_table_entries[i];

        // Dummy operation: fill memory
        sleep(rand() % 3);

        printf("Process %d used page %d\n", process->process_id, page_index);
    }

    printf("Process %d performed memory operations on %d page(s).\n", process->process_id, process->num_pages);
}

void load_pages(Process *process) {
    for (int i = 0; i < process->num_pages; i++) {
        int page_index = process->page_table_entries[i];
        load_page(page_index);
    }
}

void free_all_memory() {
    for (int i = 0; i < NUM_PAGES; i++) {
        if (page_table[i].valid) {
            free(page_table[i].frame_address);
            page_table[i].frame_address = NULL;
            page_table[i].valid = 0;
            memory[i] = NULL;
        }
    }
}

int main() {
    srand(time(NULL));

    // Initialize page table
    for (int i = 0; i < NUM_PAGES; i++) {
        page_table[i].page_number = i;
        page_table[i].valid = 0;
        page_table[i].frame_address = NULL;
        memory[i] = NULL;
    }

    int num_processes;
    printf("Enter the number of processes: ");
    scanf("%d", &num_processes);

    Process *processes = (Process *)malloc(num_processes * sizeof(Process));
    if (processes == NULL) {
        fprintf(stderr, "Could not allocate process table\n");
        return 1;
    }

    int next_free_page = 0;

    for (int i = 0; i < num_processes; i++) {
        printf("Enter memory size for process %d (in bytes): ", i + 1);
        int size;
        scanf("%d", &size);

        processes[i].process_id = i + 1;
        processes[i].memory_size = size;
        processes[i].num_pages = (int)ceil((double)size / PAGE_SIZE);

        // Check if enough physical pages are left
        if (next_free_page + processes[i].num_pages > NUM_PAGES) {
            fprintf(stderr, "Not enough memory to allocate %d pages for process %d\n",
                    processes[i].num_pages, processes[i].process_id);
            processes[i].page_table_entries = NULL;
            continue;
        }

        processes[i].page_table_entries = (int *)malloc(processes[i].num_pages * sizeof(int));
        if (processes[i].page_table_entries == NULL) {
            fprintf(stderr, "Failed to allocate page table for process %d\n", processes[i].process_id);
            continue;
        }

        for (int j = 0; j < processes[i].num_pages; j++) {
            int page_index = next_free_page++;
            processes[i].page_table_entries[j] = page_index;
        }

        load_pages(&processes[i]);
    }

    for (int i = 0; i < num_processes; i++) {
        simulate_operations(&processes[i]);
    }

    printf("\n=== Page Allocation Summary ===\n");
    printf("Total pages used: %d / %d\n", next_free_page, NUM_PAGES);
    for (int i = 0; i < num_processes; i++) {
        if (processes[i].page_table_entries == NULL) continue;

        printf("Process %d used page(s): ", processes[i].process_id);
        for (int j = 0; j < processes[i].num_pages; j++) {
            printf("%d ", processes[i].page_table_entries[j]);
        }
        printf("\n");
    }
    printf("\n");

    // Cleanup
    free_all_memory();

    for (int i = 0; i < num_processes; i++) {
        if (processes[i].page_table_entries != NULL) {
            free(processes[i].page_table_entries);
            printf("Freed page table for process %d\n", processes[i].process_id);
        }
    }

    free(processes);
    return 0;
}
