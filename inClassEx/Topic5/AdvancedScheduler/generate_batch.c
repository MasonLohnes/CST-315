#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    FILE *f = fopen("process_batch.txt", "w");
    if (!f) {
        perror("Could not open file");
        return 1;
    }

    srand(time(NULL));
    int num_processes = 100;

    fprintf(f, "%d\n", num_processes);
    for (int i = 1; i <= num_processes; i++) {
        int arrival = rand() % 50;        // arrival between 0–49
        int burst = 2 + rand() % 9;       // burst between 2–10
        int priority = rand() % 5;        // priority between 0–4
        fprintf(f, "%d %d %d %d\n", i, arrival, burst, priority);
    }
}
