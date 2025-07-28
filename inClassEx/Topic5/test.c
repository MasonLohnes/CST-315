#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Structure to represent a process
struct Process {
    int processID;
    int burstTime;
    int arrivalTime;
    int remainingTime;
    int waitingTime;
    int turnaroundTime;
    int responseTime;
    int startTime;
    int endTime;
    bool isFirstResponse;
    int priority;
};

// Sort processes by arrival time and burst time
void sortByArrivalAndBurst(struct Process processes[], int n) {
    struct Process temp;
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (processes[i].arrivalTime > processes[j].arrivalTime ||
                (processes[i].arrivalTime == processes[j].arrivalTime &&
                 processes[i].burstTime > processes[j].burstTime)) {
                temp = processes[i];
                processes[i] = processes[j];
                processes[j] = temp;
            }
        }
    }
}

// Shortest Job Next (non-preemptive)
void calculateSJNMetrics(struct Process original[], int n) {
    struct Process processes[n];
    memcpy(processes, original, sizeof(struct Process) * n);

    int currentTime = 0, completed = 0, contextSwitches = 0;
    int totalBurstTime = 0;
    for (int i = 0; i < n; i++) {
        processes[i].isFirstResponse = true;
        processes[i].waitingTime = 0;
        processes[i].responseTime = -1;
        totalBurstTime += processes[i].burstTime;
    }

    while (completed < n) {
        int shortest = -1;
        int shortestBurst = __INT_MAX__;
        for (int i = 0; i < n; i++) {
            if (processes[i].arrivalTime <= currentTime && processes[i].burstTime > 0) {
                if (processes[i].burstTime < shortestBurst) {
                    shortest = i;
                    shortestBurst = processes[i].burstTime;
                }
            }
        }

        if (shortest != -1) {
            if (processes[shortest].isFirstResponse) {
                processes[shortest].startTime = currentTime;
                processes[shortest].responseTime = currentTime - processes[shortest].arrivalTime;
                processes[shortest].isFirstResponse = false;
            }

            currentTime += processes[shortest].burstTime;
            processes[shortest].endTime = currentTime;
            processes[shortest].turnaroundTime = processes[shortest].endTime - processes[shortest].arrivalTime;
            processes[shortest].waitingTime = processes[shortest].turnaroundTime - processes[shortest].burstTime;
            processes[shortest].burstTime = 0;

            completed++;
            if (completed < n) contextSwitches++;
        } else {
            currentTime++;
        }
    }

    printf("\nSJN Scheduling:\n");
    printf("PID  Arrival  Burst  Waiting  Turnaround  Response\n");

    int totalWT = 0, totalTAT = 0, totalRT = 0;
    int minArrival = processes[0].arrivalTime;
    int maxEnd = processes[0].endTime;

    for (int i = 0; i < n; i++) {
        printf("%3d     %3d     %3d     %3d        %3d        %3d\n",
               processes[i].processID, processes[i].arrivalTime, original[i].burstTime,
               processes[i].waitingTime, processes[i].turnaroundTime, processes[i].responseTime);

        totalWT += processes[i].waitingTime;
        totalTAT += processes[i].turnaroundTime;
        totalRT += processes[i].responseTime;

        if (processes[i].arrivalTime < minArrival)
            minArrival = processes[i].arrivalTime;
        if (processes[i].endTime > maxEnd)
            maxEnd = processes[i].endTime;
    }

    float avgWT = (float)totalWT / n;
    float avgTAT = (float)totalTAT / n;
    float avgRT = (float)totalRT / n;
    float throughput = (float)n / (maxEnd - minArrival);
    float cpuUtil = ((float)totalBurstTime / (maxEnd - minArrival)) * 100;

    printf("Average Waiting Time = %.2f\n", avgWT);
    printf("Average Turnaround Time = %.2f\n", avgTAT);
    printf("Average Response Time = %.2f\n", avgRT);
    printf("Throughput = %.2f processes/unit time\n", throughput);
    printf("CPU Utilization = %.2f%%\n", cpuUtil);
    printf("Context Switches = %d\n", contextSwitches);
}

// Round Robin scheduling
void calculateRRMetrics(struct Process original[], int n, int quantum) {
    struct Process processes[n];
    memcpy(processes, original, sizeof(struct Process) * n);

    int currentTime = 0, contextSwitches = 0, completed = 0;
    int minArrival = processes[0].arrivalTime, maxEnd = 0;
    int totalBurstTime = 0;

    for (int i = 0; i < n; i++) {
        processes[i].remainingTime = processes[i].burstTime;
        processes[i].isFirstResponse = true;
        processes[i].responseTime = -1;
        totalBurstTime += processes[i].burstTime;
        if (processes[i].arrivalTime < minArrival)
            minArrival = processes[i].arrivalTime;
    }

    while (completed < n) {
        bool didExecute = false;
        for (int i = 0; i < n; i++) {
            if (processes[i].arrivalTime <= currentTime && processes[i].remainingTime > 0) {
                if (processes[i].isFirstResponse) {
                    processes[i].startTime = currentTime;
                    processes[i].responseTime = currentTime - processes[i].arrivalTime;
                    processes[i].isFirstResponse = false;
                }

                int timeSlice = (processes[i].remainingTime > quantum) ? quantum : processes[i].remainingTime;
                processes[i].remainingTime -= timeSlice;
                currentTime += timeSlice;

                if (processes[i].remainingTime == 0) {
                    processes[i].endTime = currentTime;
                    processes[i].turnaroundTime = currentTime - processes[i].arrivalTime;
                    processes[i].waitingTime = processes[i].turnaroundTime - original[i].burstTime;
                    completed++;
                }

                contextSwitches++;
                didExecute = true;
            }
        }

        if (!didExecute)
            currentTime++;
    }

    maxEnd = currentTime;

    printf("\nRound Robin Scheduling (Quantum = %d):\n", quantum);
    printf("PID  Arrival  Burst  Waiting  Turnaround  Response\n");

    int totalWT = 0, totalTAT = 0, totalRT = 0;

    for (int i = 0; i < n; i++) {
        printf("%3d     %3d     %3d     %3d        %3d        %3d\n",
               processes[i].processID, processes[i].arrivalTime, original[i].burstTime,
               processes[i].waitingTime, processes[i].turnaroundTime, processes[i].responseTime);

        totalWT += processes[i].waitingTime;
        totalTAT += processes[i].turnaroundTime;
        totalRT += processes[i].responseTime;
    }

    float avgWT = (float)totalWT / n;
    float avgTAT = (float)totalTAT / n;
    float avgRT = (float)totalRT / n;
    float throughput = (float)n / (maxEnd - minArrival);
    float cpuUtil = ((float)totalBurstTime / (maxEnd - minArrival)) * 100;

    printf("Average Waiting Time = %.2f\n", avgWT);
    printf("Average Turnaround Time = %.2f\n", avgTAT);
    printf("Average Response Time = %.2f\n", avgRT);
    printf("Throughput = %.2f processes/unit time\n", throughput);
    printf("CPU Utilization = %.2f%%\n", cpuUtil);
    printf("Context Switches = %d\n", contextSwitches - 1);
}

// Non-preemptive Priority Scheduling
void calculatePriorityMetrics(struct Process original[], int n) {
    struct Process processes[n];
    memcpy(processes, original, sizeof(struct Process) * n);

    int currentTime = 0, completed = 0, contextSwitches = 0;
    int totalBurstTime = 0;
    for (int i = 0; i < n; i++) {
        processes[i].isFirstResponse = true;
        processes[i].responseTime = -1;
        totalBurstTime += processes[i].burstTime;
    }

    int minArrival = processes[0].arrivalTime, maxEnd = 0;

    while (completed < n) {
        int highestPriorityIndex = -1;
        int highestPriority = __INT_MAX__;
        for (int i = 0; i < n; i++) {
            if (processes[i].arrivalTime <= currentTime && processes[i].burstTime > 0) {
                if (processes[i].priority < highestPriority ||
                    (processes[i].priority == highestPriority && 
                     processes[i].arrivalTime < processes[highestPriorityIndex].arrivalTime)) {
                    highestPriorityIndex = i;
                    highestPriority = processes[i].priority;
                }
            }
        }

        if (highestPriorityIndex != -1) {
            if (processes[highestPriorityIndex].isFirstResponse) {
                processes[highestPriorityIndex].startTime = currentTime;
                processes[highestPriorityIndex].responseTime = currentTime - processes[highestPriorityIndex].arrivalTime;
                processes[highestPriorityIndex].isFirstResponse = false;
            }

            currentTime += processes[highestPriorityIndex].burstTime;
            processes[highestPriorityIndex].endTime = currentTime;
            processes[highestPriorityIndex].turnaroundTime = processes[highestPriorityIndex].endTime - processes[highestPriorityIndex].arrivalTime;
            processes[highestPriorityIndex].waitingTime = processes[highestPriorityIndex].startTime - processes[highestPriorityIndex].arrivalTime;
            processes[highestPriorityIndex].burstTime = 0;

            completed++;
            if (completed < n) contextSwitches++;
        } else {
            currentTime++;
        }
    }

    for (int i = 0; i < n; i++) {
        if (processes[i].arrivalTime < minArrival)
            minArrival = processes[i].arrivalTime;
        if (processes[i].endTime > maxEnd)
            maxEnd = processes[i].endTime;
    }

    printf("\nPriority Scheduling (Non-preemptive):\n");
    printf("PID  Arrival  Burst  Priority  Waiting  Turnaround  Response\n");

    int totalWT = 0, totalTAT = 0, totalRT = 0;

    for (int i = 0; i < n; i++) {
        printf("%3d     %3d     %3d     %3d        %3d        %3d        %3d\n",
               processes[i].processID, processes[i].arrivalTime, original[i].burstTime,
               processes[i].priority, processes[i].waitingTime, processes[i].turnaroundTime, processes[i].responseTime);

        totalWT += processes[i].waitingTime;
        totalTAT += processes[i].turnaroundTime;
        totalRT += processes[i].responseTime;
    }

    float avgWT = (float)totalWT / n;
    float avgTAT = (float)totalTAT / n;
    float avgRT = (float)totalRT / n;
    float throughput = (float)n / (maxEnd - minArrival);
    float cpuUtil = ((float)totalBurstTime / (maxEnd - minArrival)) * 100;

    printf("Average Waiting Time = %.2f\n", avgWT);
    printf("Average Turnaround Time = %.2f\n", avgTAT);
    printf("Average Response Time = %.2f\n", avgRT);
    printf("Throughput = %.2f processes/unit time\n", throughput);
    printf("CPU Utilization = %.2f%%\n", cpuUtil);
    printf("Context Switches = %d\n", contextSwitches);
}

int main() {
    int n, quantum;

    printf("Enter number of processes: ");
    scanf("%d", &n);

    struct Process processes[n];

    for (int i = 0; i < n; i++) {
        processes[i].processID = i + 1;
        printf("Enter burst time for process %d: ", i + 1);
        scanf("%d", &processes[i].burstTime);
        printf("Enter arrival time for process %d: ", i + 1);
        scanf("%d", &processes[i].arrivalTime);
    }

    for (int i = 0; i < n; i++) {
        printf("Enter priority for process %d (lower number = higher priority): ", i + 1);
        scanf("%d", &processes[i].priority);
    }

    printf("Enter time quantum for Round Robin (enter 0 to skip): ");
    scanf("%d", &quantum);

    if (quantum > 0)
        calculateRRMetrics(processes, n, quantum);

    sortByArrivalAndBurst(processes, n);
    calculateSJNMetrics(processes, n);
    calculatePriorityMetrics(processes, n);

    return 0;
}
