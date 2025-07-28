#include <stdio.h>
#include <stdlib.h>

// Structure to represent a process
struct Process {
    int processID;
    int burstTime;
    int remainingTime;
    int waitingTime;
    int turnaroundTime;
};

// Function to calculate waiting time and turnaround time using Round Robin scheduling
void findWaitingTimeAndTurnaroundTime(struct Process processes[], int n, int quantum) {
    int currentTime = 0;
    int done;
    
    do {
        done = 1;
        
        for (int i = 0; i < n; i++) {
            if (processes[i].remainingTime > 0) {
                done = 0;
                if (processes[i].remainingTime > quantum) {
                    currentTime += quantum;
                    processes[i].remainingTime -= quantum;
                } else {
                    currentTime += processes[i].remainingTime;
                    processes[i].waitingTime = currentTime - processes[i].burstTime;
                    processes[i].remainingTime = 0;
                }
            }
        }
    } while (!done);
    
    for (int i = 0; i < n; i++) {
        processes[i].turnaroundTime = processes[i].burstTime + processes[i].waitingTime;
    }
}

// Function to calculate average waiting and turnaround times
void findAvgTime(struct Process processes[], int n, int quantum) {
    int totalWaitingTime = 0, totalTurnaroundTime = 0;

    findWaitingTimeAndTurnaroundTime(processes, n, quantum);

    printf("Processes  Burst Time  Waiting Time  Turnaround Time\n");

    for (int i = 0; i < n; i++) {
        totalWaitingTime += processes[i].waitingTime;
        totalTurnaroundTime += processes[i].turnaroundTime;
        printf("   %d \t\t %d \t\t %d \t\t %d\n", processes[i].processID, processes[i].burstTime, processes[i].waitingTime, processes[i].turnaroundTime);
    }

    printf("Average Waiting Time = %.2f\n", (float)totalWaitingTime / (float)n);
    printf("Average Turnaround Time = %.2f\n", (float)totalTurnaroundTime / (float)n);
}

int main() {
    int n, quantum;

    printf("Enter the number of processes: ");
    scanf("%d", &n);

    struct Process processes[n];

    for (int i = 0; i < n; i++) {
        processes[i].processID = i + 1;
        printf("Enter burst time for process %d: ", i + 1);
        scanf("%d", &processes[i].burstTime);
        processes[i].remainingTime = processes[i].burstTime;
    }

    printf("Enter time quantum: ");
    scanf("%d", &quantum);

    findAvgTime(processes, n, quantum);

    return 0;
}
