// Mason Lohnes, CST-315, Assignment 3: Deadlock Avoidance
// 	This code creates multiple threads all trying to get the same resource,
// though a mutex keeps it exclusive. If a process cannot get the resource
// for 5 seconds, it starves and restarts.
//
// All of this is output, and a run is logged in output.txt.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define NUM_PROCESSES 5

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
	int process_id;
        int restart_count;
} process_info;

void *run_process(void *arg) {
	process_info *info = (process_info *)arg;
	int acquired = 0;

	while (!acquired) {
                printf("Process %d trying to access the resource...\n", info->process_id);

                time_t start = time(NULL);

                // Try acquiring the lock for up to 5 seconds
                while (time(NULL) - start < 5) {
                        if (pthread_mutex_trylock(&lock) == 0) {
                                acquired = 1;
                                break;
                        }
                        sleep(1);
                }

                if (acquired) {
                        printf("Access granted to process %d. Using resource...\n", info->process_id);
                        sleep(3);
                        pthread_mutex_unlock(&lock);
                        printf("Process %d released the resource. Completed Process.\n", info->process_id);
                } else {
                        printf("Process %d Starved. Restarting process...\n", info->process_id);
                        info->restart_count++;
                        sleep(1);
                }
        }

        return NULL;
}

int main() {
	pthread_t processes[NUM_PROCESSES];
	process_info infos[NUM_PROCESSES];

	for (int i = 0; i < NUM_PROCESSES; i++) {
		infos[i].process_id = i + 1;
		infos[i].restart_count = 0;
		pthread_create(&processes[i], NULL, run_process, &infos[i]);
        }

	for (int i = 0; i < NUM_PROCESSES; i++) {
                pthread_join(processes[i], NULL);
        }

        printf("\n=== Simulation Summary ===\n");
        for (int i = 0; i < NUM_PROCESSES; i++) {
                printf("Process %d was restarted %d time(s) due to starvation.\n",
                        infos[i].process_id, infos[i].restart_count);
        }
}
