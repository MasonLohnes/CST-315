#include <stdio.h>
#include <stdlib.h>

// Structure to represent a process
struct Process {
    int processID;
    int burstTime;
    int waitingTime;
    int turnaroundTime;
};

// Function to find the waiting time for all processes
void findWaitingTime(struct Process processes[], int n) {
    processes[0].waitingTime = 0; // First process has no waiting time

    for (int i = 1; i < n; i++) {
        processes[i].waitingTime = processes[i-1].waitingTime + processes[i-1].burstTime;
    }
}

// Function to find the turnaround time for all processes
void findTurnaroundTime(struct Process processes[], int n) {
    for (int i = 0; i < n; i++) {
        processes[i].turnaroundTime = processes[i].waitingTime + processes[i].burstTime;
    }
}

// Function to calculate average waiting and turnaround times
void findAvgTime(struct Process processes[], int n) {
    int totalWaitingTime = 0, totalTurnaroundTime = 0;

    findWaitingTime(processes, n);
    findTurnaroundTime(processes, n);

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
    int n;

    printf("Enter the number of processes: ");
    scanf("%d", &n);

    struct Process processes[n];

    for (int i = 0; i < n; i++) {
        processes[i].processID = i + 1;
        printf("Enter burst time for process %d: ", i + 1);
        scanf("%d", &processes[i].burstTime);
    }

    findAvgTime(processes, n);

    return 0;
}
