// Mason Lohnes, CST-315, Combined Shell with Process Scheduler and File Management
// Unified Shell: Simple Round Robin + Priority + Aging Scheduler + File Operations

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
#include <libgen.h>
#include <time.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_COMMANDS 10
#define MAX_PROCESSES 64
#define TIME_SLICE 100000      // 100ms - simple fixed time slice
#define AGING_BOOST 5          // Priority boost every 5 cycles
#define IO_TIME 200000         // 200ms I/O simulation

// VMM Constants
#define PAGE_SIZE 4096
#define PHYSICAL_FRAMES 16
#define VIRTUAL_PAGES 64
#define SWAP_SLOTS 32

// Process states
typedef enum {
    PROC_NEW,
    PROC_READY,
    PROC_RUNNING,
    PROC_WAITING,
    PROC_TERMINATED
} ProcessState;

// VMM structures
typedef struct {
    int frame_number;
    int is_present;
    int is_dirty;
    int swap_slot;
} page_entry_t;

typedef struct {
    int is_used;
    int process_id;
    int page_number;
    int load_time;
} frame_entry_t;

typedef struct {
    int pid;
    int memory_size;
    int num_pages;
    page_entry_t *page_table;
} process_info_t;

typedef struct {
    frame_entry_t frames[PHYSICAL_FRAMES];
    process_info_t processes[MAX_PROCESSES];
    int swap_used[SWAP_SLOTS];
    int next_frame_time;
    int num_processes;
} vmm_t;

// PCB
typedef struct PCB {
    int pid;
    char command[64];
    ProcessState state;
    int priority; // 0=high, 1=normal, 2=low
    int cpu_time;
    int wait_time;
    int arrival_time;
    int last_run;
    int age_counter;
    int io_count;
    int memory_allocated;
    struct PCB* next;
} PCB;

// Simple queue structure
typedef struct {
    PCB* head;
    int count;
    pthread_mutex_t lock;
} ProcessQueue;

// Simple scheduler
typedef struct {
    ProcessQueue ready;     // All ready processes
    ProcessQueue waiting;   // I/O waiting
    PCB* running;
    int total_procs;
    int done_procs;
    long total_wait;
    long total_turnaround;
    int scheduler_on;
    pthread_t sched_thread;
} SimpleScheduler;

// Global variables
vmm_t vmm;
SimpleScheduler sched = {0};
int vmm_verbose = 0;
int scheduler_verbose = 0;
struct termios original_term;
pid_t foreground_pgid = 0;
volatile sig_atomic_t ctrl_x_pressed = 0;
int is_interactive = 0;

// VMM Implementation
void init_vmm() {
    if (vmm_verbose) {
        printf("Initializing VMM: %d frames, %d KB pages\n", 
               PHYSICAL_FRAMES, PAGE_SIZE/1024);
    }

    for (int i = 0; i < PHYSICAL_FRAMES; i++) {
        vmm.frames[i].is_used = 0;
        vmm.frames[i].process_id = -1;
    }

    for (int i = 0; i < MAX_PROCESSES; i++) {
        vmm.processes[i].pid = -1;
        vmm.processes[i].page_table = NULL;
    }

    for (int i = 0; i < SWAP_SLOTS; i++) {
        vmm.swap_used[i] = 0;
    }

    vmm.next_frame_time = 1;
    vmm.num_processes = 0;
}

int allocate_process_memory(int pid, int memory_size) {
    int proc_index = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (vmm.processes[i].pid == -1) {
            proc_index = i;
            break;
        }
    }

    if (proc_index == -1) return -1;

    int pages_needed = (memory_size + PAGE_SIZE - 1) / PAGE_SIZE;
    page_entry_t *page_table = malloc(pages_needed * sizeof(page_entry_t));
    if (!page_table) return -1;

    for (int i = 0; i < pages_needed; i++) {
        page_table[i].is_present = 0;
        page_table[i].frame_number = -1;
    }

    vmm.processes[proc_index].pid = pid;
    vmm.processes[proc_index].memory_size = memory_size;
    vmm.processes[proc_index].num_pages = pages_needed;
    vmm.processes[proc_index].page_table = page_table;
    vmm.num_processes++;

    if (vmm_verbose) {
        printf("VMM: Allocated %d KB (%d pages) for PID %d\n", 
               memory_size/1024, pages_needed, pid);
    }
    return 0;
}

void deallocate_process_memory(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (vmm.processes[i].pid == pid) {
            if (vmm.processes[i].page_table) {
                free(vmm.processes[i].page_table);
            }
            vmm.processes[i].pid = -1;
            vmm.processes[i].page_table = NULL;
            vmm.num_processes--;

            if (vmm_verbose) {
                printf("VMM: Deallocated memory for PID %d\n", pid);
            }
            break;
        }
    }
}

// Scheduler Implementation
long get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

void enqueue(ProcessQueue* q, PCB* p) {
    pthread_mutex_lock(&q->lock);
    p->next = NULL;
    if (!q->head) {
        q->head = p;
    } else {
        PCB* curr = q->head;
        while (curr->next) curr = curr->next;
        curr->next = p;
    }
    q->count++;

    if (scheduler_verbose) {
        printf("Scheduler: Enqueued PID %d (priority %d)\n", p->pid, p->priority);
    }
    pthread_mutex_unlock(&q->lock);
}

PCB* dequeue_by_priority(ProcessQueue* q) {
    pthread_mutex_lock(&q->lock);

    if (!q->head) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    // Find highest priority process (lowest number = highest priority)
    PCB* best = q->head;
    PCB* best_prev = NULL;
    PCB* curr = q->head;
    PCB* prev = NULL;

    while (curr) {
        if (curr->priority < best->priority) {
            best = curr;
            best_prev = prev;
        }
        prev = curr;
        curr = curr->next;
    }

    // Remove best from queue
    if (best_prev) {
        best_prev->next = best->next;
    } else {
        q->head = best->next;
    }
    q->count--;

    pthread_mutex_unlock(&q->lock);
    return best;
}

PCB* create_process(int pid, const char* cmd, int memory_size) {
    PCB* p = malloc(sizeof(PCB));
    if (!p) return NULL;

    p->pid = pid;
    strncpy(p->command, cmd, 63);
    p->command[63] = '\0';
    p->state = PROC_NEW;
    p->priority = 1;        // Start normal priority
    p->cpu_time = 0;
    p->wait_time = 0;
    p->arrival_time = get_time();
    p->last_run = 0;
    p->age_counter = 0;
    p->io_count = 0;
    p->memory_allocated = memory_size;
    p->next = NULL;

    // Allocate memory through VMM
    if (allocate_process_memory(pid, memory_size) != 0) {
        free(p);
        return NULL;
    }

    sched.total_procs++;

    if (scheduler_verbose) {
        printf("Scheduler: Created PID %d (%s) with %d KB\n", 
               pid, cmd, memory_size/1024);
    }

    return p;
}

void* scheduler_main(void* arg) {
    while (sched.scheduler_on) {
        long now = get_time();

        // Handle I/O completion
        pthread_mutex_lock(&sched.waiting.lock);
        PCB* curr = sched.waiting.head;
        PCB* prev = NULL;
        while (curr) {
            if (now - curr->last_run >= IO_TIME) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    sched.waiting.head = curr->next;
                }
                sched.waiting.count--;

                PCB* ready_proc = curr;
                curr = curr->next;
                ready_proc->state = PROC_READY;
                ready_proc->priority = 0;  // I/O means higher priority
                ready_proc->wait_time += now - ready_proc->last_run;

                pthread_mutex_unlock(&sched.waiting.lock);
                enqueue(&sched.ready, ready_proc);
                pthread_mutex_lock(&sched.waiting.lock);

                if (scheduler_verbose) {
                    printf("Scheduler: PID %d I/O completed, priority boosted\n", ready_proc->pid);
                }
            } else {
                prev = curr;
                curr = curr->next;
            }
        }
        pthread_mutex_unlock(&sched.waiting.lock);

        // Check for preemption
        if (sched.running && (now - sched.running->last_run >= TIME_SLICE)) {
            PCB* preempted = sched.running;
            sched.running = NULL;
            preempted->state = PROC_READY;
            preempted->cpu_time += now - preempted->last_run;

            // Simple I/O simulation
            if (rand() % 4 == 0) {  // 25% chance
                preempted->state = PROC_WAITING;
                preempted->last_run = now;
                preempted->io_count++;
                enqueue(&sched.waiting, preempted);

                if (scheduler_verbose) {
                    printf("Scheduler: PID %d moved to I/O wait\n", preempted->pid);
                }
            } else {
                if (preempted->priority < 2) preempted->priority++;
                enqueue(&sched.ready, preempted);

                if (scheduler_verbose) {
                    printf("Scheduler: PID %d preempted, priority now %d\n", 
                           preempted->pid, preempted->priority);
                }
            }
        }

        // Age processes
        pthread_mutex_lock(&sched.ready.lock);
        curr = sched.ready.head;
        while (curr) {
            curr->age_counter++;
            if (curr->age_counter >= AGING_BOOST) {
                curr->age_counter = 0;
                if (curr->priority > 0) {
                    curr->priority--;
                    if (scheduler_verbose) {
                        printf("Scheduler: PID %d aged up to priority %d\n", 
                               curr->pid, curr->priority);
                    }
                }
            }
            curr = curr->next;
        }
        pthread_mutex_unlock(&sched.ready.lock);
        
        // Select next process
        if (!sched.running) {
            PCB* next = dequeue_by_priority(&sched.ready);
            if (next) {
                sched.running = next;
                next->state = PROC_RUNNING;
                next->last_run = now;
                
                if (scheduler_verbose) {
                    printf("Scheduler: Running PID %d (priority %d)\n", 
                           next->pid, next->priority);
                }
            }
        }
        usleep(10000);  // 10ms scheduling quantum
    }
    return NULL;
}

void finish_process(PCB* p) {
    if (sched.running == p) sched.running = NULL;
    
    long now = get_time();
    p->state = PROC_TERMINATED;
    sched.done_procs++;
    sched.total_turnaround += now - p->arrival_time;
    sched.total_wait += p->wait_time;
    
    deallocate_process_memory(p->pid);
    
    if (scheduler_verbose) {
        printf("Scheduler: PID %d finished (CPU: %dms, Wait: %dms, I/O: %d)\n", 
               p->pid, p->cpu_time/1000, p->wait_time/1000, p->io_count);
    }
    
    free(p);
}

// File Management Functions
void create_file(const char *path, int random_size) {
    size_t size = random_size ?
        (rand() % (10 * 1024 * 1024 - 1024)) + 1024 :  // Random 1KB-10MB
        1024;                                           // Default 1KB

    printf("Creating %s (%s%zu bytes)\n",
           path, random_size ? "random " : "", size);

    FILE *file = fopen(path, "w");
    if (!file) {
        perror("Failed to create file");
        return;
    }

    unsigned char *data = malloc(size);
    if (!data) {
        perror("Memory allocation failed");
        fclose(file);
        return;
    }

    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }

    fwrite(data, 1, size, file);
    free(data);
    fclose(file);
}

void modify_file(const char *path) {
    printf("Modifying %s\n", path);
    FILE *file = fopen(path, "a");
    if (file == NULL) {
        perror("Failed to open file for modification");
        return;
    }
    fprintf(file, "Appended content to the file.\n");
    fclose(file);
}

void delete_file(const char *path) {
    printf("Deleting %s\n", path);
    if (remove(path) != 0) {
        perror("Failed to delete file");
    }
}

void get_file_info(const char *path, int detailed) {
    struct stat st;
    if (stat(path, &st) != 0) {
        perror("Error getting file info");
        return;
    }

    printf("\nInformation for: %s\n", path);
    printf("Type: Regular File\n");
    printf("Size: %ld bytes\n", st.st_size);
    printf("Permissions: %o\n", st.st_mode & 0777);
    printf("Last modified: %s", ctime(&st.st_mtime));

    if (detailed) {
        printf("\n-- Detailed Information --\n");
        printf("Inode: %ld\n", st.st_ino);
        printf("Hard Links: %ld\n", st.st_nlink);
        printf("Owner UID: %d\n", st.st_uid);
        printf("Group GID: %d\n", st.st_gid);
        printf("Device: %ld\n", st.st_dev);
        printf("Last access: %s", ctime(&st.st_atime));
        printf("Last status change: %s", ctime(&st.st_ctime));
    }
    printf("\n");
}

void create_directory(const char *path) {
    if (mkdir(path, 0755) == 0) {
        printf("Directory '%s' created successfully.\n", path);
    } else {
        perror("newdir failed");
    }
}

void delete_directory_contents(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("Failed to open directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            delete_directory_contents(full_path);
            rmdir(full_path);
        } else {
            unlink(full_path);
        }
    }
    closedir(dir);
}

void delete_directory(const char *path, int recursive) {
    if (recursive) {
        delete_directory_contents(path);
    }

    if (rmdir(path) == 0) {
        printf("Directory '%s' deleted successfully.\n", path);
    } else {
        if (recursive) {
            perror("Failed to delete directory (even with -r)");
        } else {
            printf("Directory not empty. Use -r to delete recursively.\n");
        }
    }
}

void printTree(const char *base_path, int depth) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(base_path);
    if (!dir) {
        perror("opendir failed");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        for (int i = 0; i < depth; i++)
            printf("│   ");
        printf("├── %s\n", entry->d_name);

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            printTree(path, depth + 1);
        }
    }
    closedir(dir);
}

void renameItem(const char *oldName, const char *newName) {
    if (rename(oldName, newName) == 0) {
        printf("Renamed '%s' to '%s'\n", oldName, newName);
    } else {
        perror("rename failed");
    }
}

void moveItem(const char *item, const char *newPath) {
    const char *filename = strrchr(item, '/');
    filename = (filename == NULL) ? item : filename + 1;

    char fullDestPath[1024];
    snprintf(fullDestPath, sizeof(fullDestPath), "%s/%s", newPath, filename);

    if (rename(item, fullDestPath) == 0) {
        printf("Moved '%s' to '%s'\n", item, fullDestPath);
    } else {
        perror("move failed");
    }
}

const char *get_basename(const char *path) {
    const char *last_slash = strrchr(path, '/');
    return last_slash ? last_slash + 1 : path;
}

void auto_copy(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        perror("Error checking path");
        return;
    }

    char newname[256];
    char command[512];
    int counter = 1;

    const char *base = get_basename(path);
    const char *dot = strrchr(base, '.');

    do {
        if (dot && S_ISREG(st.st_mode)) {
            snprintf(newname, sizeof(newname), "%.*s(%d)%s",
                    (int)(dot - base), base, counter, dot);
        } else {
            snprintf(newname, sizeof(newname), "%s(%d)", base, counter);
        }
        counter++;
    } while (access(newname, F_OK) == 0);

    if (S_ISDIR(st.st_mode)) {
        snprintf(command, sizeof(command), "cp -r \"%s\" \"%s\"", path, newname);
    } else {
        snprintf(command, sizeof(command), "cp \"%s\" \"%s\"", path, newname);
    }

    system(command);
    printf("Created copy: %s\n", newname);
}

int search_file(const char *dir_path, const char *target_file, int *found) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        if (errno != EACCES) {
            perror("Failed to open directory");
        }
        return 0;
    }

    struct dirent *entry;
    char path[PATH_MAX];
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        if (strcmp(entry->d_name, target_file) == 0) {
            printf("Found: %s\n", path);
            (*found)++;
            count++;
        }

        if (entry->d_type == DT_DIR) {
            count += search_file(path, target_file, found);
        }
    }
    closedir(dir);
    return count;
}

// Process Management Functions
void print_processes(int detailed, int sort_id) {
    printf("\n=== Process Table ===\n");
    if (detailed) {
        printf("%-6s %-15s %-10s %-3s %-8s %-8s %-4s %-4s %-8s\n",
               "PID", "Command", "State", "PRI", "CPU(ms)", "Wait(ms)", "I/O", "AGE", "Mem(KB)");
        printf("-----------------------------------------------------------------------\n");
    } else {
        printf("%-6s %-15s %-10s %-3s\n", "PID", "Command", "State", "PRI");
        printf("-----------------------------------\n");
    }

    PCB* procs[MAX_PROCESSES];
    int count = 0;

    if (sched.running) procs[count++] = sched.running;

    pthread_mutex_lock(&sched.ready.lock);
    PCB* curr = sched.ready.head;
    while (curr && count < MAX_PROCESSES) {
        procs[count++] = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&sched.ready.lock);

    pthread_mutex_lock(&sched.waiting.lock);
    curr = sched.waiting.head;
    while (curr && count < MAX_PROCESSES) {
        procs[count++] = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&sched.waiting.lock);

    if (sort_id) {
        for (int i = 0; i < count-1; i++) {
            for (int j = i+1; j < count; j++) {
                if (procs[i]->pid > procs[j]->pid) {
                    PCB* temp = procs[i];
                    procs[i] = procs[j];
                    procs[j] = temp;
                }
            }
        }
    }

    for (int i = 0; i < count; i++) {
        PCB* p = procs[i];
        const char* state_str;

        switch (p->state) {
            case PROC_NEW: state_str = "NEW"; break;
            case PROC_READY: state_str = "READY"; break;
            case PROC_RUNNING: state_str = "RUNNING"; break;
            case PROC_WAITING: state_str = "WAITING"; break;
            case PROC_TERMINATED: state_str = "DONE"; break;
            default: state_str = "UNKNOWN"; break;
        }
        
        if (detailed) {
            printf("%-6d %-15s %-10s %-3d %-8d %-8d %-4d %-4d %-8d\n",
                   p->pid, p->command, state_str, p->priority,
                   p->cpu_time/1000, p->wait_time/1000, 
                   p->io_count, p->age_counter, p->memory_allocated/1024);
        } else {
            printf("%-6d %-15s %-10s %-3d\n", p->pid, p->command, state_str, p->priority);
        }
    }
    
    printf("\nTotal: %d, Active: %d, Done: %d\n", sched.total_procs, count, sched.done_procs);
    if (sched.done_procs > 0) {
        printf("Avg Turnaround: %ldms, Avg Wait: %ldms\n",
               sched.total_turnaround/sched.done_procs/1000,
               sched.total_wait/sched.done_procs/1000);
    }
    printf("===================\n\n");
}

void set_priority(int pid, int new_pri) {
    if (new_pri < 0 || new_pri > 2) {
        printf("Priority must be 0 (HIGH), 1 (NORMAL), or 2 (LOW)\n");
        return;
    }
    
    if (sched.running && sched.running->pid == pid) {
        sched.running->priority = new_pri;
        printf("Set running PID %d priority to %d\n", pid, new_pri);
        return;
    }
    
    pthread_mutex_lock(&sched.ready.lock);
    PCB* curr = sched.ready.head;
    while (curr) {
        if (curr->pid == pid) {
            curr->priority = new_pri;
            curr->age_counter = 0;
            printf("Set PID %d priority to %d\n", pid, new_pri);
            pthread_mutex_unlock(&sched.ready.lock);
            return;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&sched.ready.lock);
    
    pthread_mutex_lock(&sched.waiting.lock);
    curr = sched.waiting.head;
    while (curr) {
        if (curr->pid == pid) {
            curr->priority = new_pri;
            printf("Set waiting PID %d priority to %d\n", pid, new_pri);
            pthread_mutex_unlock(&sched.waiting.lock);
            return;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&sched.waiting.lock);
    
    printf("Process PID %d not found\n", pid);
}

void print_scheduler_stats() {
    printf("\n=== Scheduler Statistics ===\n");
    printf("Algorithm: Round Robin + Priority + Aging\n");
    printf("Time Slice: %d ms\n", TIME_SLICE/1000);
    printf("Aging: Priority boost every %d cycles\n", AGING_BOOST);
    printf("I/O Time: %d ms simulation\n", IO_TIME/1000);
    printf("\nProcess Counts:\n");
    printf("  Total Created: %d\n", sched.total_procs);
    printf("  Completed: %d\n", sched.done_procs);
    printf("  Active: %d\n", sched.total_procs - sched.done_procs);
    printf("  Ready Queue: %d\n", sched.ready.count);
    printf("  I/O Waiting: %d\n", sched.waiting.count);
    
    if (sched.running) {
        printf("  Currently Running: PID %d (%s)\n", 
               sched.running->pid, sched.running->command);
    } else {
        printf("  Currently Running: None\n");
    }
    
    if (sched.done_procs > 0) {
        printf("\nPerformance:\n");
        printf("  Average Turnaround: %ld ms\n", 
               sched.total_turnaround/sched.done_procs/1000);
        printf("  Average Wait Time: %ld ms\n", 
               sched.total_wait/sched.done_procs/1000);
    }
    printf("===========================\n\n");
}

void print_vmm_status() {
    printf("\n=== VMM Status ===\n");
    
    int used_frames = 0;
    for (int i = 0; i < PHYSICAL_FRAMES; i++) {
        if (vmm.frames[i].is_used) used_frames++;
    }
    
    printf("Physical Memory: %d/%d frames used (%d%%)\n", 
           used_frames, PHYSICAL_FRAMES, (used_frames * 100) / PHYSICAL_FRAMES);
    printf("Active VMM Processes: %d\n", vmm.num_processes);
    printf("Memory Utilization: %d KB / %d KB\n", 
           (used_frames * PAGE_SIZE) / 1024, 
           (PHYSICAL_FRAMES * PAGE_SIZE) / 1024);
    printf("==================\n\n");
}

// Shell Interface Functions
void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_term);
}

void handle_sigint(int sig) {
    if (foreground_pgid > 0) {
        kill(-foreground_pgid, SIGINT);
    }
    ctrl_x_pressed = 1;
    restore_terminal();
}

void disable_canonical_mode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &original_term);
    raw = original_term;
    raw.c_lflag &= ~(ICANON);
    raw.c_lflag &= ~ECHO;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void parse_commands(char *input, char **commands) {
    char *token;
    int i = 0;

    for (int j = 0; j < MAX_COMMANDS; j++) {
        commands[j] = NULL;
    }

    token = strtok(input, ";");
    while (token != NULL && i < MAX_COMMANDS - 1) {
        while (isspace(*token)) token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace(*end)) end--;
        *(end + 1) = '\0';

        if (strlen(token) > 0) {
            commands[i++] = token;
        }
        token = strtok(NULL, ";");
    }
}

void parse_args(char *command, char **args) {
    char *token;
    int i = 0;

    for (int j = 0; j < MAX_ARGS; j++) {
        args[j] = NULL;
    }

    token = strtok(command, " \t\r\n");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\r\n");
    }
}

void print_help() {
    printf("=== Combined Shell Commands ===\n\n");
    
    printf("PROCESS MANAGEMENT:\n");
    printf("  procs         - List processes (basic)\n");
    printf("  procs -a      - List processes (detailed)\n");
    printf("  procs -a -si  - List processes (detailed, sorted by ID)\n");
    printf("  priority <pid> <0-2> - Set process priority (0=HIGH, 1=NORMAL, 2=LOW)\n");
    printf("  stats         - Show scheduler statistics\n");
    printf("  vmm           - Toggle VMM verbose output (currently: %s)\n", vmm_verbose ? "ON" : "OFF");
    printf("  sched         - Toggle scheduler verbose output (currently: %s)\n", scheduler_verbose ? "ON" : "OFF");
    
    printf("\nFILE OPERATIONS:\n");
    printf("  create [-f] <file> - Create file (use -f for random size)\n");
    printf("  modify <file>      - Modify file by appending content\n");
    printf("  delete <file>      - Delete file\n");
    printf("  finf [-d] <file>   - Get file information (use -d for details)\n");
    printf("  copy <target>      - Copy file/directory with auto-numbering\n");
    printf("  rename <old> <new> - Rename file or directory\n");
    printf("  move <src> <dest>  - Move file/directory\n");
    printf("  search <file>      - Search for file in directory tree\n");
    
    printf("\nDIRECTORY OPERATIONS:\n");
    printf("  newdir <dir>       - Create new directory\n");
    printf("  killdir [-r] <dir> - Delete directory (use -r for recursive)\n");
    printf("  dinf [-d] <dir>    - Get directory info (use -d for details)\n");
    printf("  tree [dir]         - Show directory tree structure\n");
    
    printf("\nGENERAL:\n");
    printf("  cd [dir]      - Change directory\n");
    printf("  help          - Show this help message\n");
    printf("  quit/Ctrl+X   - Exit shell\n");
    printf("  command &     - Run command in background\n\n");
    
    printf("SCHEDULER INFO:\n");
    printf("  Algorithm: Round Robin + Priority + Aging\n");
    printf("  Time Slice: %dms, Aging: every %d cycles\n", TIME_SLICE/1000, AGING_BOOST);
    printf("  Priority Levels: 0=HIGH, 1=NORMAL, 2=LOW\n");
    printf("================================\n\n");
}

void check_background_processes() {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int found = 0;
        
        if (sched.running && sched.running->pid == pid) {
            finish_process(sched.running);
            found = 1;
        } else {
            pthread_mutex_lock(&sched.ready.lock);
            PCB* curr = sched.ready.head;
            PCB* prev = NULL;
            
            while (curr && !found) {
                if (curr->pid == pid) {
                    if (prev) {
                        prev->next = curr->next;
                    } else {
                        sched.ready.head = curr->next;
                    }
                    sched.ready.count--;
                    finish_process(curr);
                    found = 1;
                } else {
                    prev = curr;
                    curr = curr->next;
                }
            }
            pthread_mutex_unlock(&sched.ready.lock);
            
            if (!found) {
                pthread_mutex_lock(&sched.waiting.lock);
                curr = sched.waiting.head;
                prev = NULL;
                
                while (curr) {
                    if (curr->pid == pid) {
                        if (prev) {
                            prev->next = curr->next;
                        } else {
                            sched.waiting.head = curr->next;
                        }
                        sched.waiting.count--;
                        finish_process(curr);
                        found = 1;
                        break;
                    }
                    prev = curr;
                    curr = curr->next;
                }
                pthread_mutex_unlock(&sched.waiting.lock);
            }
        }
        
        if (found) {
            printf("[Background process %d completed]\n", pid);
        }
    }
}

void execute_commands(char **commands) {
    int num_commands = 0;
    pid_t pids[MAX_COMMANDS];
    int is_background[MAX_COMMANDS] = {0};

    while (commands[num_commands] != NULL && num_commands < MAX_COMMANDS) {
        num_commands++;
    }

    for (int i = 0; i < num_commands; i++) {
        char *args[MAX_ARGS];
        parse_args(commands[i], args);

        if (args[0] != NULL) {
            // Check for background process
            int arg_count = 0;
            while (args[arg_count] != NULL) arg_count++;
            
            if (arg_count > 0 && strcmp(args[arg_count-1], "&") == 0) {
                is_background[i] = 1;
                args[arg_count-1] = NULL;
                printf("Running in background: %s\n", args[0]);
            }

            // Handle built-in commands
            if (strcmp(args[0], "quit") == 0) {
                printf("Exiting shell...\n");
                sched.scheduler_on = 0;
                restore_terminal();
                pthread_join(sched.sched_thread, NULL);
                exit(0);
            }
            else if (strcmp(args[0], "help") == 0) {
                print_help();
                continue;
            }
            else if (strcmp(args[0], "cd") == 0) {
                if (args[1] == NULL) {
                    char cwd[1024];
                    if (getcwd(cwd, sizeof(cwd)) != NULL) {
                        printf("Current directory: %s\n", cwd);
                    } else {
                        perror("getcwd failed");
                    }
                } else if (strcmp(args[1], "HOME") == 0) {
                    const char *home = getenv("HOME");
                    if (home == NULL) home = "/";
                    if (chdir(home) != 0) {
                        perror("cd HOME failed");
                    }
                } else {
                    if (chdir(args[1]) != 0) {
                        perror("cd failed");
                    }
                }
                continue;
            }
            // Process management commands
            else if (strcmp(args[0], "procs") == 0) {
                int detailed = 0;
                int sort_id = 0;

                for (int j = 1; args[j] != NULL; j++) {
                    if (strcmp(args[j], "-a") == 0) {
                        detailed = 1;
                    } else if (strcmp(args[j], "-si") == 0) {
                        sort_id = 1;
                    }
                }
                print_processes(detailed, sort_id);
                continue;
            }
            else if (strcmp(args[0], "priority") == 0) {
                if (args[1] && args[2]) {
                    int pid = atoi(args[1]);
                    int priority = atoi(args[2]);
                    set_priority(pid, priority);
                } else {
                    printf("Usage: priority <pid> <priority>\n");
                    printf("Priority levels: 0=HIGH, 1=NORMAL, 2=LOW\n");
                }
                continue;
            }
            else if (strcmp(args[0], "stats") == 0) {
                print_scheduler_stats();
                continue;
            }
            else if (strcmp(args[0], "vmm") == 0) {
                vmm_verbose = !vmm_verbose;
                printf("VMM verbose output: %s\n", vmm_verbose ? "ON" : "OFF");
                if (vmm_verbose) {
                    print_vmm_status();
                }
                continue;
            }
            else if (strcmp(args[0], "sched") == 0) {
                scheduler_verbose = !scheduler_verbose;
                printf("Scheduler verbose output: %s\n", scheduler_verbose ? "ON" : "OFF");
                continue;
            }
            // File operations
            else if (strcmp(args[0], "create") == 0) {
                if (args[1] == NULL) {
                    printf("Usage: create [-f] <file1> [file2...]\n");
                    continue;
                }

                int random_size = 0;
                int file_arg_start = 1;

                if (strcmp(args[1], "-f") == 0) {
                    if (args[2] == NULL) {
                        printf("Error: -f requires filename\n");
                        continue;
                    }
                    random_size = 1;
                    file_arg_start = 2;
                }

                static int seeded = 0;
                if (!seeded) {
                    srand(time(NULL));
                    seeded = 1;
                }

                for (int j = file_arg_start; args[j] != NULL; j++) {
                    create_file(args[j], random_size);
                }
                continue;
            }
            else if (strcmp(args[0], "modify") == 0) {
                if (args[1] == NULL) {
                    printf("Usage: modify <file1> [file2...]\n");
                    continue;
                }
                for (int j = 1; args[j] != NULL; j++) {
                    modify_file(args[j]);
                }
                continue;
            }
            else if (strcmp(args[0], "delete") == 0) {
                if (args[1] == NULL) {
                    printf("Usage: delete <file1> [file2...]\n");
                    continue;
                }
                for (int j = 1; args[j] != NULL; j++) {
                    delete_file(args[j]);
                }
                continue;
            }
            else if (strcmp(args[0], "finf") == 0) {
                if (args[1] == NULL) {
                    printf("Usage: finf [-d] <file>\n");
                    continue;
                }

                int detailed = 0;
                const char *target = args[1];

                if (strcmp(args[1], "-d") == 0) {
                    if (args[2] == NULL) {
                        printf("Error: No file specified after -d\n");
                        continue;
                    }
                    detailed = 1;
                    target = args[2];
                }

                get_file_info(target, detailed);
                continue;
            }
            else if (strcmp(args[0], "copy") == 0) {
                if (args[1] == NULL) {
                    printf("Usage: copy <file_or_directory>\n");
                    continue;
                }

                for (int j = 1; args[j] != NULL; j++) {
                    auto_copy(args[j]);
                }
                continue;
            }
            else if (strcmp(args[0], "rename") == 0) {
                if (args[1] == NULL || args[2] == NULL) {
                    printf("Usage: rename <oldname> <newname>\n");
                    continue;
                }
                renameItem(args[1], args[2]);
                continue;
            }
            else if (strcmp(args[0], "move") == 0) {
                if (args[1] == NULL || args[2] == NULL) {
                    printf("Usage: move <source> <destination>\n");
                    continue;
                }
                moveItem(args[1], args[2]);
                continue;
            }
            else if (strcmp(args[0], "search") == 0) {
                if (args[1] == NULL) {
                    printf("Usage: search <filename>\n");
                    continue;
                }

                char cwd[PATH_MAX];
                if (getcwd(cwd, sizeof(cwd)) == NULL) {
                    perror("Failed to get current directory");
                    continue;
                }

                printf("Searching for '%s' in %s and subdirectories...\n", args[1], cwd);

                int found = 0;
                search_file(cwd, args[1], &found);

                if (found == 0) {
                    printf("No matches found for '%s'\n", args[1]);
                } else {
                    printf("Found %d match(es)\n", found);
                }
                continue;
            }
            // Directory operations
            else if (strcmp(args[0], "newdir") == 0) {
                if (args[1] == NULL) {
                    printf("Usage: newdir <directory1> [directory2...]\n");
                    continue;
                }
                for (int j = 1; args[j] != NULL; j++) {
                    create_directory(args[j]);
                }
                continue;
            }
            else if (strcmp(args[0], "killdir") == 0) {
                if (args[1] == NULL) {
                    printf("Usage: killdir [-r] <directory>...\n");
                    continue;
                }

                int recursive = 0;
                int j = 1;

                if (strcmp(args[1], "-r") == 0) {
                    recursive = 1;
                    j = 2;
                    if (args[j] == NULL) {
                        printf("No directories specified after -r\n");
                        continue;
                    }
                }

                while (args[j] != NULL) {
                    delete_directory(args[j], recursive);
                    j++;
                }
                continue;
            }
            else if (strcmp(args[0], "dinf") == 0) {
                if (args[1] == NULL) {
                    printf("Usage: dinf [-d] <directory>\n");
                    continue;
                }

                int detailed = 0;
                const char *target = args[1];

                if (strcmp(args[1], "-d") == 0) {
                    if (args[2] == NULL) {
                        printf("Error: No directory specified after -d\n");
                        continue;
                    }
                    detailed = 1;
                    target = args[2];
                }

                struct stat st;
                if (stat(target, &st) != 0) {
                    perror("Error getting directory info");
                    continue;
                }

                printf("\nInformation for: %s\n", target);
                printf("Type: Directory\n");
                printf("Permissions: %o\n", st.st_mode & 0777);
                printf("Last modified: %s", ctime(&st.st_mtime));

                if (detailed) {
                    DIR *dir = opendir(target);
                    int file_count = 0;
                    int dir_count = 0;
                    long total_size = 0;
                    struct dirent *entry;

                    printf("\n-- Detailed Information --\n");
                    printf("Inode: %ld\n", st.st_ino);
                    printf("Hard Links: %ld\n", st.st_nlink);
                    printf("Owner UID: %d\n", st.st_uid);
                    printf("Group GID: %d\n", st.st_gid);
                    printf("Device: %ld\n", st.st_dev);

                    if (dir) {
                        while ((entry = readdir(dir)) != NULL) {
                            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                                continue;

                            char full_path[PATH_MAX];
                            snprintf(full_path, sizeof(full_path), "%s/%s", target, entry->d_name);

                            struct stat entry_st;
                            if (stat(full_path, &entry_st) == 0) {
                                if (S_ISDIR(entry_st.st_mode)) {
                                    dir_count++;
                                } else {
                                    file_count++;
                                    total_size += entry_st.st_size;
                                }
                            }
                        }
                        closedir(dir);

                        printf("Contents: %d files, %d subdirectories\n", file_count, dir_count);
                        printf("Total size of files: %ld bytes\n", total_size);
                    }
                    printf("Last access: %s", ctime(&st.st_atime));
                    printf("Last status change: %s", ctime(&st.st_ctime));
                }
                printf("\n");
                continue;
            }
            else if (strcmp(args[0], "tree") == 0) {
                if (args[1] == NULL) {
                    char cwd[PATH_MAX];
                    getcwd(cwd, sizeof(cwd));
                    printTree(cwd, 0);
                } else {
                    printTree(args[1], 0);
                }
                continue;
            }

            // Fork and execute external command
            pids[i] = fork();

            if (pids[i] < 0) {
                perror("Fork failed");
                continue;
            }

            if (pids[i] == 0) {
                // Child process
                setpgid(0, 0);
                signal(SIGINT, SIG_DFL);
                execvp(args[0], args);
                fprintf(stderr, "Execution failed: %s\n", strerror(errno));
                fflush(stderr);
                exit(1);
            } else {
                // Parent - create PCB and add to scheduler
                int memory_size;
                if (strcmp(args[0], "ls") == 0) {
                    memory_size = 8 * 1024;   // 8KB
                } else if (strcmp(args[0], "whoami") == 0) {
                    memory_size = 4 * 1024;   // 4KB
                } else if (strcmp(args[0], "who") == 0) {
                    memory_size = 12 * 1024;  // 12KB
                } else if (strcmp(args[0], "pwd") == 0) {
                    memory_size = 6 * 1024;   // 6KB
                } else if (strcmp(args[0], "date") == 0) {
                    memory_size = 8 * 1024;   // 8KB
                } else if (strcmp(args[0], "ps") == 0) {
                    memory_size = 16 * 1024;  // 16KB
                } else if (strcmp(args[0], "cat") == 0) {
                    memory_size = 12 * 1024;  // 12KB
                } else if (strcmp(args[0], "sleep") == 0) {
                    memory_size = 4 * 1024;   // 4KB for sleep
                } else {
                    memory_size = 10 * 1024;  // 10KB default
                }
                
                PCB* process = create_process(pids[i], args[0], memory_size);
                if (process) {
                    process->state = PROC_READY;
                    enqueue(&sched.ready, process);
                }
                
                if (!is_background[i] && i == 0) {
                    foreground_pgid = pids[i];
                    if (is_interactive) {
                        tcsetpgrp(STDIN_FILENO, foreground_pgid);
                    }
                }
                setpgid(pids[i], is_background[i] ? pids[i] : foreground_pgid);
            }
        }
    }

    // Wait for foreground processes
    for (int i = 0; i < num_commands; i++) {
        if (commands[i] != NULL && !is_background[i]) {
            int status;
            pid_t finished_pid = waitpid(pids[i], &status, 0);
            
            if (finished_pid > 0) {
                int found = 0;
                
                if (sched.running && sched.running->pid == finished_pid) {
                    finish_process(sched.running);
                    found = 1;
                } else {
                    pthread_mutex_lock(&sched.ready.lock);
                    PCB* curr = sched.ready.head;
                    PCB* prev = NULL;
                    
                    while (curr && !found) {
                        if (curr->pid == finished_pid) {
                            if (prev) {
                                prev->next = curr->next;
                            } else {
                                sched.ready.head = curr->next;
                            }
                            sched.ready.count--;
                            finish_process(curr);
                            found = 1;
                        } else {
                            prev = curr;
                            curr = curr->next;
                        }
                    }
                    pthread_mutex_unlock(&sched.ready.lock);
                    
                    if (!found) {
                        pthread_mutex_lock(&sched.waiting.lock);
                        curr = sched.waiting.head;
                        prev = NULL;
                        
                        while (curr) {
                            if (curr->pid == finished_pid) {
                                if (prev) {
                                    prev->next = curr->next;
                                } else {
                                    sched.waiting.head = curr->next;
                                }
                                sched.waiting.count--;
                                finish_process(curr);
                                found = 1;
                                break;
                            }
                            prev = curr;
                            curr = curr->next;
                        }
                        pthread_mutex_unlock(&sched.waiting.lock);
                    }
                }
            }
            
            if (ctrl_x_pressed) {
                printf("\nExiting shell...\n");
                sched.scheduler_on = 0;
                restore_terminal();
                pthread_join(sched.sched_thread, NULL);
                exit(0);
            }
        }
    }

    if (is_interactive) {
        tcsetpgrp(STDIN_FILENO, getpid());
        disable_canonical_mode();
    }

    foreground_pgid = 0;
}

void process_batch_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening batch file");
        exit(1);
    }

    char line[MAX_LINE];
    char *commands[MAX_COMMANDS];

    printf("Processing batch file: %s\n", filename);
    printf("Combined Shell: Scheduler + File Management\n\n");

    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }

        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) == 0) {
            continue;
        }

        printf("Executing: %s\n", line);

        memset(commands, 0, sizeof(commands));
        parse_commands(line, commands);
        if (commands[0] != NULL) {
            execute_commands(commands);
        }
        
        usleep(50000);  // 50ms pause between commands
    }

    fclose(file);
    
    printf("\nWaiting for all processes to complete...\n");
    while (sched.total_procs > sched.done_procs) {
        check_background_processes();
        usleep(100000);  // 100ms
        if (scheduler_verbose) {
            printf("Active processes: %d\n", 
                   sched.total_procs - sched.done_procs);
        }
    }
    
    print_scheduler_stats();
    sched.scheduler_on = 0;
    pthread_join(sched.sched_thread, NULL);
    exit(0);
}

void interactive_mode() {
    char input[MAX_LINE];
    char *commands[MAX_COMMANDS];
    int pos = 0;
    char c;

    signal(SIGINT, handle_sigint);
    signal(SIGTTOU, SIG_IGN);

    disable_canonical_mode();
    atexit(restore_terminal);

    printf("=== Lope Shell ===\n");
    printf("Features: File Management + Process Scheduling + VMM\n");
    printf("Type 'help' for available commands.\n\n");

    while (1) {
        check_background_processes();

        char cwd[PATH_MAX];
        getcwd(cwd, sizeof(cwd));
        printf("$lopeShell:%s$ ", cwd);
        fflush(stdout);
        pos = 0;
        ctrl_x_pressed = 0;

        while (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 24) {  // Ctrl+X
                printf("\nExiting shell...\n");
                sched.scheduler_on = 0;
                pthread_join(sched.sched_thread, NULL);
                exit(0);
            }

            if (ctrl_x_pressed) {
                printf("\nExiting shell...\n");
                sched.scheduler_on = 0;
                pthread_join(sched.sched_thread, NULL);
                exit(0);
            }

            if (c == '\n') {
                input[pos] = '\0';
                write(STDOUT_FILENO, "\n", 1);
                fflush(stdout);
                break;
            }
            else if (c == 127) {  // Backspace
                if (pos > 0) {
                    pos--;
                    write(STDOUT_FILENO, "\b \b", 3);
                    fflush(stdout);
                }
            }
            else if (pos < MAX_LINE - 1 && isprint(c)) {
                input[pos++] = c;
                write(STDOUT_FILENO, &c, 1);
                fflush(stdout);
            }
        }

        if (ctrl_x_pressed) {
            printf("\nExiting shell...\n");
            sched.scheduler_on = 0;
            pthread_join(sched.sched_thread, NULL);
            exit(0);
        }

        if (pos == 0) continue;

        if (strcmp(input, "quit") == 0) {
            printf("Exiting shell...\n");
            sched.scheduler_on = 0;
            pthread_join(sched.sched_thread, NULL);
            exit(0);
        }

        memset(commands, 0, sizeof(commands));
        parse_commands(input, commands);
        if (commands[0] != NULL) {
            execute_commands(commands);
        }
    }
}

void cleanup_resources() {
    sched.scheduler_on = 0;
    if (sched.sched_thread) {
        pthread_join(sched.sched_thread, NULL);
    }
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (vmm.processes[i].page_table != NULL) {
            free(vmm.processes[i].page_table);
        }
    }
    
    printf("Resources cleaned up.\n");
}

void init_scheduler() {
    pthread_mutex_init(&sched.ready.lock, NULL);
    pthread_mutex_init(&sched.waiting.lock, NULL);
    sched.scheduler_on = 1;
    sched.total_procs = 0;
    sched.done_procs = 0;
    sched.total_wait = 0;
    sched.total_turnaround = 0;
    sched.running = NULL;
    sched.ready.head = NULL;
    sched.ready.count = 0;
    sched.waiting.head = NULL;
    sched.waiting.count = 0;
    
    pthread_create(&sched.sched_thread, NULL, scheduler_main, NULL);
    
    if (scheduler_verbose) {
        printf("Combined Scheduler initialized:\n");
        printf("  Algorithm: Round Robin + Priority + Aging\n");
        printf("  Time Slice: %dms\n", TIME_SLICE/1000);
        printf("  Aging: Every %d cycles\n", AGING_BOOST);
        printf("  Preemptive: YES\n");
    }
}

int main(int argc, char *argv[]) {
    // Initialize systems
    init_vmm();
    init_scheduler();
    
    // Set up cleanup
    atexit(cleanup_resources);
    
    printf("=== Lope Shell ===\n");
    printf("VMM: %d frames (%d KB), Scheduler: RR+Priority+Aging\n", 
           PHYSICAL_FRAMES, (PHYSICAL_FRAMES * PAGE_SIZE) / 1024);

    if (argc > 1) {
        is_interactive = 0;
        process_batch_file(argv[1]);
    } else {
        is_interactive = 1;
        interactive_mode();
    }

    restore_terminal();
    return 0;
}
