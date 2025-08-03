// Mason Lohnes, CST-315, Shell with Simple Virtual Memory Manager
// Project 4: Added simple VMM with FIFO paging
// 	The visualization of the paging system can be turned on and off
// with "vmm". The system is overall very rudimentary and runs off of a
// first in first out system. The total amounts of memory can be read
// under VMM amounts.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_COMMANDS 10

// VMM amounts
#define PAGE_SIZE 4096
#define PHYSICAL_FRAMES 16
#define VIRTUAL_PAGES 64
#define MAX_PROCESSES 8
#define SWAP_SLOTS 32

// page states
#define PAGE_FREE 0
#define PAGE_USED 1
#define PAGE_SWAPPED 2

// page table entry
typedef struct {
    int frame_number;
    int is_present;
    int is_dirty;
    int swap_slot;
} page_entry_t;

// frame table entry
typedef struct {
    int is_used;          
    int process_id;       
    int page_number;      
    int load_time;        
} frame_entry_t;

// process memory info
typedef struct {
    int pid;
    int memory_size;      
    int num_pages;        
    page_entry_t *page_table;  
} process_info_t;

// vmm state
typedef struct {
    frame_entry_t frames[PHYSICAL_FRAMES];    
    process_info_t processes[MAX_PROCESSES];  
    int swap_used[SWAP_SLOTS];                
    int next_frame_time;                      
    int num_processes;
} vmm_t;

// global vmm instance
vmm_t vmm;
int vmm_verbose = 0;  // toggle for vmm output - starts off

// shell variables
struct termios original_term;
pid_t foreground_pgid = 0;
volatile sig_atomic_t ctrl_x_pressed = 0;
int is_interactive = 0;

void init_vmm() {
    if (vmm_verbose) {
        printf("Initializing Simple Virtual Memory Manager...\n");
        printf("Physical Memory: %d frames (%d KB)\n", PHYSICAL_FRAMES, (PHYSICAL_FRAMES * PAGE_SIZE) / 1024);
        printf("Virtual Memory: %d pages (%d KB)\n", VIRTUAL_PAGES, (VIRTUAL_PAGES * PAGE_SIZE) / 1024);
        printf("Page Size: %d bytes\n", PAGE_SIZE);
        printf("Algorithm: FIFO\n");
    }
    
    // init frame table
    for (int i = 0; i < PHYSICAL_FRAMES; i++) {
        vmm.frames[i].is_used = 0;
        vmm.frames[i].process_id = -1;
        vmm.frames[i].page_number = -1;
        vmm.frames[i].load_time = 0;
    }
    
    // init process table
    for (int i = 0; i < MAX_PROCESSES; i++) {
        vmm.processes[i].pid = -1;
        vmm.processes[i].memory_size = 0;
        vmm.processes[i].num_pages = 0;
        vmm.processes[i].page_table = NULL;
    }
    
    // init swap slots
    for (int i = 0; i < SWAP_SLOTS; i++) {
        vmm.swap_used[i] = 0;
    }
    
    vmm.next_frame_time = 1;
    vmm.num_processes = 0;
    
    if (vmm_verbose) {
        printf("VMM initialization complete.\n\n");
    }
}

void cleanup_vmm() {
    if (vmm_verbose) {
        printf("Cleaning up VMM...\n");
    }
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (vmm.processes[i].page_table != NULL) {
            free(vmm.processes[i].page_table);
        }
    }
}

int allocate_process_memory(int pid, int memory_size) {
    // find empty process slot
    int proc_index = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (vmm.processes[i].pid == -1) {
            proc_index = i;
            break;
        }
    }
    
    if (proc_index == -1) {
        if (vmm_verbose) printf("VMM Error: No free process slots\n");
        return -1;
    }
    
    // calculate pages needed
    int pages_needed = (memory_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    if (vmm_verbose) {
        printf("VMM: Allocating memory for process %d\n", pid);
        printf("     Memory requested: %d bytes\n", memory_size);
        printf("     Pages needed: %d\n", pages_needed);
    }
    
    // allocate page table
    page_entry_t *page_table = malloc(pages_needed * sizeof(page_entry_t));
    if (!page_table) {
        if (vmm_verbose) printf("VMM Error: Cannot allocate page table\n");
        return -1;
    }
    
    // init page table entries (demand paging)
    for (int i = 0; i < pages_needed; i++) {
        page_table[i].frame_number = -1;
        page_table[i].is_present = 0;
        page_table[i].is_dirty = 0;
        page_table[i].swap_slot = -1;
    }
    
    // store process info
    vmm.processes[proc_index].pid = pid;
    vmm.processes[proc_index].memory_size = memory_size;
    vmm.processes[proc_index].num_pages = pages_needed;
    vmm.processes[proc_index].page_table = page_table;
    vmm.num_processes++;
    
    if (vmm_verbose) {
        printf("     Process allocated successfully\n");
    }
    return 0;
}

void deallocate_process_memory(int pid) {
    // find process
    int proc_index = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (vmm.processes[i].pid == pid) {
            proc_index = i;
            break;
        }
    }
    
    if (proc_index == -1) return;
    
    if (vmm_verbose) {
        printf("VMM: Deallocating memory for process %d\n", pid);
    }
    
    // free all frames used by this process
    for (int i = 0; i < PHYSICAL_FRAMES; i++) {
        if (vmm.frames[i].process_id == pid) {
            vmm.frames[i].is_used = 0;
            vmm.frames[i].process_id = -1;
            vmm.frames[i].page_number = -1;
            vmm.frames[i].load_time = 0;
        }
    }
    
    // free swap slots used by this process
    process_info_t *proc = &vmm.processes[proc_index];
    for (int i = 0; i < proc->num_pages; i++) {
        if (proc->page_table[i].swap_slot != -1) {
            vmm.swap_used[proc->page_table[i].swap_slot] = 0;
        }
    }
    
    // free page table and reset process info
    free(proc->page_table);
    proc->pid = -1;
    proc->memory_size = 0;
    proc->num_pages = 0;
    proc->page_table = NULL;
    vmm.num_processes--;
}

int find_free_frame() {
    for (int i = 0; i < PHYSICAL_FRAMES; i++) {
        if (!vmm.frames[i].is_used) {
            return i;
        }
    }
    return -1;
}

int swap_in_page(int swap_slot, int frame_index) {
    // simulate reading from swap
    if (vmm_verbose) {
        printf("VMM: Reading page from swap slot %d into frame %d\n", swap_slot, frame_index);
    }
    // in real implementation this would read actual data from swap file
    return 0;
}

void swap_out_page(int frame_index) {
    frame_entry_t *frame = &vmm.frames[frame_index];
    
    // find the process and page table entry
    int proc_index = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (vmm.processes[i].pid == frame->process_id) {
            proc_index = i;
            break;
        }
    }
    
    if (proc_index == -1) return;
    
    process_info_t *proc = &vmm.processes[proc_index];
    page_entry_t *page = &proc->page_table[frame->page_number];
    
    // only write to swap if page is dirty
    if (page->is_dirty) {
        if (vmm_verbose) printf("VMM: Page is dirty, writing to swap\n");
        
        // find free swap slot
        int swap_slot = -1;
        for (int i = 0; i < SWAP_SLOTS; i++) {
            if (!vmm.swap_used[i]) {
                swap_slot = i;
                vmm.swap_used[i] = 1;
                break;
            }
        }
        
        if (swap_slot != -1) {
            page->swap_slot = swap_slot;
            if (vmm_verbose) printf("VMM: Page written to swap slot %d\n", swap_slot);
        }
    } else {
        if (vmm_verbose) printf("VMM: Page is clean, not writing to swap\n");
    }
    
    // update page table
    page->frame_number = -1;
    page->is_present = 0;
    
    // clear frame
    frame->is_used = 0;
    frame->process_id = -1;
    frame->page_number = -1;
    frame->load_time = 0;
}

int evict_page_fifo() {
    // find oldest frame (fifo)
    int oldest_frame = 0;
    int oldest_time = vmm.frames[0].load_time;
    
    for (int i = 1; i < PHYSICAL_FRAMES; i++) {
        if (vmm.frames[i].load_time < oldest_time) {
            oldest_time = vmm.frames[i].load_time;
            oldest_frame = i;
        }
    }
    
    if (vmm_verbose) {
        printf("VMM: Evicting frame %d (PID=%d, Page=%d)\n", 
               oldest_frame, 
               vmm.frames[oldest_frame].process_id,
               vmm.frames[oldest_frame].page_number);
    }
    
    swap_out_page(oldest_frame);
    return oldest_frame;
}

int handle_page_fault(int pid, int virtual_page) {
    if (vmm_verbose) {
        printf("VMM: Page fault - PID=%d, Page=%d\n", pid, virtual_page);
    }
    
    // find process
    int proc_index = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (vmm.processes[i].pid == pid) {
            proc_index = i;
            break;
        }
    }
    
    if (proc_index == -1) {
        if (vmm_verbose) printf("VMM Error: Process not found\n");
        return -1;
    }
    
    process_info_t *proc = &vmm.processes[proc_index];
    
    // check if page is valid
    if (virtual_page >= proc->num_pages) {
        if (vmm_verbose) printf("VMM Error: Segmentation fault - invalid page access\n");
        return -1;
    }
    
    page_entry_t *page = &proc->page_table[virtual_page];
    
    // find free frame or evict one
    int frame_index = find_free_frame();
    if (frame_index == -1) {
        if (vmm_verbose) printf("VMM: No free frames, need to evict\n");
        frame_index = evict_page_fifo();
    }
    
    if (frame_index == -1) {
        if (vmm_verbose) printf("VMM Error: Cannot get frame\n");
        return -1;
    }
    
    // load page into frame
    if (page->swap_slot != -1) {
        // page is in swap, load it
        if (vmm_verbose) printf("VMM: Loading page from swap slot %d\n", page->swap_slot);
        swap_in_page(page->swap_slot, frame_index);
        vmm.swap_used[page->swap_slot] = 0;
        page->swap_slot = -1;
    } else {
        // first time access
        if (vmm_verbose) printf("VMM: First access to page, allocating frame %d\n", frame_index);
    }
    
    // update page table
    page->frame_number = frame_index;
    page->is_present = 1;
    
    // update frame table
    vmm.frames[frame_index].is_used = 1;
    vmm.frames[frame_index].process_id = pid;
    vmm.frames[frame_index].page_number = virtual_page;
    vmm.frames[frame_index].load_time = vmm.next_frame_time++;
    
    if (vmm_verbose) {
        printf("VMM: Page fault resolved\n");
    }
    return 0;
}

void print_vmm_status() {
    printf("\n=== VMM Status ===\n");
    
    // show frame usage in detail
    printf("Frame Table:\n");
    for (int i = 0; i < PHYSICAL_FRAMES; i++) {
        if (vmm.frames[i].is_used) {
            printf("  Frame %d: PID=%d, Page=%d, Time=%d\n", 
                   i, vmm.frames[i].process_id, 
                   vmm.frames[i].page_number, vmm.frames[i].load_time);
        } else {
            printf("  Frame %d: FREE\n", i);
        }
    }
    
    int used_frames = 0;
    for (int i = 0; i < PHYSICAL_FRAMES; i++) {
        if (vmm.frames[i].is_used) {
            used_frames++;
        }
    }
    printf("Physical frames used: %d/%d\n", used_frames, PHYSICAL_FRAMES);
    
    printf("Swap slots used: ");
    int used_swap = 0;
    for (int i = 0; i < SWAP_SLOTS; i++) {
        if (vmm.swap_used[i]) {
            used_swap++;
        }
    }
    printf("%d/%d\n", used_swap, SWAP_SLOTS);
    
    printf("Active processes: %d\n", vmm.num_processes);
    printf("Next frame time: %d\n", vmm.next_frame_time);
    printf("==================\n\n");
}

void handle_sigint(int sig) {
    if (foreground_pgid > 0) {
        kill(-foreground_pgid, SIGINT);
    }
    ctrl_x_pressed = 1;
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

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_term);
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
    printf("Built-in commands:\n");
    printf("  help          - Show this help message\n");
    printf("  cd [dir]      - Change the current directory to [dir]\n");
    printf("  cd HOME       - Change the current directory to the home directory\n");
    printf("  cd            - Shows the current directory\n");
    printf("  vmm           - Toggle VMM verbose output (currently: %s)\n", vmm_verbose ? "ON" : "OFF");
    printf("  fillmem       - Fill memory to demonstrate FIFO eviction\n");
    printf("  quit/Ctrl + X - Exit the shell\n");
    printf("  Ctrl + C      - Cancel process\n");
    printf("You can also run any executable in your PATH.\n");
}

void execute_commands(char **commands) {
    int num_commands = 0;
    pid_t pids[MAX_COMMANDS];

    while (commands[num_commands] != NULL && num_commands < MAX_COMMANDS) {
        num_commands++;
    }

    for (int i = 0; i < num_commands; i++) {
        char *args[MAX_ARGS];
        parse_args(commands[i], args);

        if (args[0] != NULL) {
            // handle built-in commands before fork
            if (strcmp(args[0], "quit") == 0) {
                printf("Exiting shell...\n");
                cleanup_vmm();
                exit(0);
            }
            else if (strcmp(args[0], "help") == 0) {
                print_help();
                continue;
            }
            else if (strcmp(args[0], "vmm") == 0) {
                // toggle vmm verbose output
                vmm_verbose = !vmm_verbose;
                printf("VMM verbose output: %s\n", vmm_verbose ? "ON" : "OFF");
                
                // show current status if turning on
                if (vmm_verbose) {
                    print_vmm_status();
                }
                continue;
            }
            else if (strcmp(args[0], "fillmem") == 0) {
                // command to fill up memory and force evictions
                printf("VMM: Filling memory to demonstrate FIFO eviction...\n");
                
                // allocate a test process that needs many pages
                allocate_process_memory(9999, 20 * PAGE_SIZE);  // 20 pages
                
                for (int p = 0; p < 20; p++) {  // try to access 20 pages
                    printf("VMM: Accessing page %d\n", p);
                    handle_page_fault(9999, p);
                    
                    // show frame status every few pages
                    if (p % 5 == 4) {
                        printf("--- Frame status after page %d ---\n", p);
                        for (int f = 0; f < PHYSICAL_FRAMES; f++) {
                            if (vmm.frames[f].is_used) {
                                printf("Frame %d: PID=%d, Page=%d, Time=%d\n", 
                                       f, vmm.frames[f].process_id, 
                                       vmm.frames[f].page_number, 
                                       vmm.frames[f].load_time);
                            }
                        }
                        printf("-------------------------------\n");
                    }
                }
                
                deallocate_process_memory(9999);
                print_vmm_status();
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

            pids[i] = fork();

            if (pids[i] < 0) {
                perror("Fork failed");
                continue;
            }

            if (pids[i] == 0) {
                // child process - allocate memory via vmm
                // small memory sizes for 1-4 pages per process
                int memory_needed;
                if (strcmp(args[0], "ls") == 0) {
                    memory_needed = 6 * 1024;   // 6kb = 2 pages
                } else if (strcmp(args[0], "whoami") == 0) {
                    memory_needed = 4 * 1024;   // 4kb = 1 page
                } else if (strcmp(args[0], "who") == 0) {
                    memory_needed = 12 * 1024;  // 12kb = 3 pages
                } else if (strcmp(args[0], "pwd") == 0) {
                    memory_needed = 8 * 1024;   // 8kb = 2 pages
                } else if (strcmp(args[0], "date") == 0) {
                    memory_needed = 16 * 1024;  // 16kb = 4 pages
                } else {
                    memory_needed = 8 * 1024;   // 8kb = 2 pages default
                }
                
                allocate_process_memory(getpid(), memory_needed);
                
                // access all pages that were allocated
                int pages_to_access = (memory_needed + PAGE_SIZE - 1) / PAGE_SIZE;
                for (int p = 0; p < pages_to_access; p++) {
                    handle_page_fault(getpid(), p);
                }
                
                setpgid(0, 0);
                signal(SIGINT, SIG_DFL);
                execvp(args[0], args);
                perror("Execution failed");
                deallocate_process_memory(getpid());
                exit(1);
            } else {
                if (i == 0) {
                    foreground_pgid = pids[i];
                    if (is_interactive) {
                        tcsetpgrp(STDIN_FILENO, foreground_pgid);
                    }
                }
                setpgid(pids[i], foreground_pgid);
            }
        }
    }

    for (int i = 0; i < num_commands; i++) {
        if (commands[i] != NULL) {
            int status;
            waitpid(pids[i], &status, 0);
            
            // clean up vmm resources for finished process
            if (vmm_verbose) {
                printf("VMM: Process %d finished, cleaning up\n", pids[i]);
            }
            deallocate_process_memory(pids[i]);
            
            if (ctrl_x_pressed) {
                printf("\nExiting shell...\n");
                cleanup_vmm();
                exit(0);
            }
        }
    }

    foreground_pgid = 0;
    if (is_interactive) {
        tcsetpgrp(STDIN_FILENO, getpid());
    }
}

void process_batch_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening batch file");
        exit(1);
    }

    char line[MAX_LINE];
    char *commands[MAX_COMMANDS];

    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }

        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) == 0) {
            continue;
        }

        printf("%s\n", line);

        memset(commands, 0, sizeof(commands));
        parse_commands(line, commands);
        if (commands[0] != NULL) {
            execute_commands(commands);
        }
    }

    fclose(file);
    cleanup_vmm();
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

    while (1) {
        printf("$lopeShell: ");
        fflush(stdout);
        pos = 0;
        ctrl_x_pressed = 0;

        while (read(STDIN_FILENO, &c, 1) == 1) {
            if (ctrl_x_pressed) {
                printf("\nExiting shell...\n");
                cleanup_vmm();
                exit(0);
            }

            if (c == '\n') {
                input[pos] = '\0';
                write(STDOUT_FILENO, "\n", 1);
                break;
            }
            else if (c == 24) {  // ctrl+x
                printf("\nExiting shell...\n");
                cleanup_vmm();
                exit(0);
            }
            else if (c == 127) {  // backspace
                if (pos > 0) {
                    pos--;
                    write(STDOUT_FILENO, "\b \b", 3);
                }
            }
            else if (pos < MAX_LINE - 1 && isprint(c)) {
                input[pos++] = c;
                write(STDOUT_FILENO, &c, 1);
            }
        }

        if (ctrl_x_pressed) {
            printf("\nExiting shell...\n");
            cleanup_vmm();
            exit(0);
        }

        if (pos == 0) continue;

        if (strcmp(input, "quit") == 0) {
            printf("Exiting shell...\n");
            cleanup_vmm();
            exit(0);
        }

        memset(commands, 0, sizeof(commands));
        parse_commands(input, commands);
        if (commands[0] != NULL) {
            execute_commands(commands);
        }
    }
}

int main(int argc, char *argv[]) {
    // init vmm
    init_vmm();
    
    if (argc > 1) {
        is_interactive = 0;
        process_batch_file(argv[1]);
    } else {
        is_interactive = 1;
        interactive_mode();
    }
    
    cleanup_vmm();
    return 0;
}
