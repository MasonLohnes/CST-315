#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

struct Process {
    int pid;
    int burst;
    int arrival;
    int remaining;
    int waiting;
    int turnaround;
    int response;
    int start;
    int end;
    bool first_response;
    int priority;
};

void print_results(struct Process p[], int n, const char* title, bool show_priority) {
    printf("\n%s:\n", title);
    if (show_priority) {
        printf("PID Arrival Burst Priority Waiting Turnaround Response\n");
    } else {
        printf("PID Arrival Burst Waiting Turnaround Response\n");
    }

    int total_wait = 0, total_turnaround = 0, total_response = 0;
    int min_arrival = p[0].arrival;
    int max_end = p[0].end;
    int total_burst = 0;

    for (int i = 0; i < n; i++) {
        if (show_priority) {
            printf("%3d %7d %5d %8d %7d %10d %9d\n",
                  p[i].pid, p[i].arrival, p[i].burst, p[i].priority,
                  p[i].waiting, p[i].turnaround, p[i].response);
        } else {
            printf("%3d %7d %5d %7d %10d %9d\n",
                  p[i].pid, p[i].arrival, p[i].burst,
                  p[i].waiting, p[i].turnaround, p[i].response);
        }

        total_wait += p[i].waiting;
        total_turnaround += p[i].turnaround;
        total_response += p[i].response;
        total_burst += p[i].burst;

        if (p[i].arrival < min_arrival) min_arrival = p[i].arrival;
        if (p[i].end > max_end) max_end = p[i].end;
    }

    float avg_wait = (float)total_wait / n;
    float avg_turnaround = (float)total_turnaround / n;
    float avg_response = (float)total_response / n;
    float throughput = (float)n / (max_end - min_arrival);
    float cpu_util = ((float)total_burst / (max_end - min_arrival)) * 100;

    printf("\nAverage Waiting Time = %.2f\n", avg_wait);
    printf("Average Turnaround Time = %.2f\n", avg_turnaround);
    printf("Average Response Time = %.2f\n", avg_response);
    printf("Throughput = %.2f processes/unit time\n", throughput);
    printf("CPU Utilization = %.2f%%\n", cpu_util);
}

void round_robin(struct Process original[], int n, int quantum) {
    struct Process processes[n];
    memcpy(processes, original, sizeof(struct Process) * n);

    int current_time = 0;
    int completed = 0;
    int switches = 0;
    int idle_time = 0;

    for (int i = 0; i < n; i++) {
        processes[i].remaining = processes[i].burst;
        processes[i].first_response = true;
        processes[i].waiting = 0;
    }

    while (completed < n) {
        bool executed = false;

        for (int i = 0; i < n; i++) {
            if (processes[i].arrival <= current_time && processes[i].remaining > 0) {
                if (processes[i].first_response) {
                    processes[i].response = current_time - processes[i].arrival;
                    processes[i].first_response = false;
                    processes[i].start = current_time;
                }

                int slice = (processes[i].remaining > quantum) ? quantum : processes[i].remaining;
                processes[i].remaining -= slice;
                current_time += slice;
                executed = true;

                if (processes[i].remaining == 0) {
                    processes[i].end = current_time;
                    processes[i].turnaround = current_time - processes[i].arrival;
                    processes[i].waiting = processes[i].start - processes[i].arrival;
                    completed++;
                }

                switches++;
            }
        }

        if (!executed) {
            current_time++;
            idle_time++;
        }
    }

    print_results(processes, n, "Round Robin Scheduling", false);
    printf("Context Switches = %d\n", switches - 1);
}

void sjn(struct Process original[], int n) {
    struct Process processes[n];
    memcpy(processes, original, sizeof(struct Process) * n);

    int current_time = 0;
    int completed = 0;
    int switches = 0;

    for (int i = 0; i < n; i++) {
        processes[i].remaining = processes[i].burst;
        processes[i].first_response = true;
        processes[i].waiting = 0;
    }

    while (completed < n) {
        int shortest = -1;
        int shortest_burst = INT_MAX;

        for (int i = 0; i < n; i++) {
            if (processes[i].arrival <= current_time && processes[i].remaining > 0) {
                if (processes[i].remaining < shortest_burst) {
                    shortest = i;
                    shortest_burst = processes[i].remaining;
                }
            }
        }

        if (shortest != -1) {
            processes[shortest].start = current_time;
            processes[shortest].waiting = current_time - processes[shortest].arrival;
            processes[shortest].response = processes[shortest].waiting;
            
            current_time += processes[shortest].remaining;
            processes[shortest].end = current_time;
            processes[shortest].turnaround = current_time - processes[shortest].arrival;
            processes[shortest].remaining = 0;
            
            completed++;
            if (completed < n) switches++;
        } else {
            current_time++;
        }
    }

    print_results(processes, n, "SJN Scheduling", false);
    printf("Context Switches = %d\n", switches);
}

void priority_scheduling(struct Process original[], int n) {
    struct Process processes[n];
    memcpy(processes, original, sizeof(struct Process) * n);

    int current_time = 0;
    int completed = 0;
    int switches = 0;

    for (int i = 0; i < n; i++) {
        processes[i].remaining = processes[i].burst;
        processes[i].first_response = true;
        processes[i].waiting = 0;
    }

    while (completed < n) {
        int highest_priority = -1;
        int highest_priority_value = INT_MAX;

        for (int i = 0; i < n; i++) {
            if (processes[i].arrival <= current_time && processes[i].remaining > 0) {
                if (processes[i].priority < highest_priority_value || 
                   (processes[i].priority == highest_priority_value && 
                    processes[i].arrival < processes[highest_priority].arrival)) {
                    highest_priority = i;
                    highest_priority_value = processes[i].priority;
                }
            }
        }

        if (highest_priority != -1) {
            processes[highest_priority].start = current_time;
            processes[highest_priority].waiting = current_time - processes[highest_priority].arrival;
            processes[highest_priority].response = processes[highest_priority].waiting;
            
            current_time += processes[highest_priority].remaining;
            processes[highest_priority].end = current_time;
            processes[highest_priority].turnaround = current_time - processes[highest_priority].arrival;
            processes[highest_priority].remaining = 0;
            
            completed++;
            if (completed < n) switches++;
        } else {
            current_time++;
        }
    }

    print_results(processes, n, "Priority Scheduling (Non-preemptive)", true);
    printf("Context Switches = %d\n", switches);
}

int main() {
    int n, quantum;

    printf("Enter number of processes: ");
    scanf("%d", &n);

    struct Process processes[n];

    for (int i = 0; i < n; i++) {
        processes[i].pid = i + 1;
        printf("Enter burst time for process %d: ", i + 1);
        scanf("%d", &processes[i].burst);
        printf("Enter arrival time for process %d: ", i + 1);
        scanf("%d", &processes[i].arrival);
    }

    for (int i = 0; i < n; i++) {
        printf("Enter priority for process %d (lower number = higher priority): ", i + 1);
        scanf("%d", &processes[i].priority);
    }

    printf("Enter time quantum for Round Robin (enter 0 to skip): ");
    scanf("%d", &quantum);

    if (quantum > 0) {
        round_robin(processes, n, quantum);
    }

    sjn(processes, n);
    priority_scheduling(processes, n);

    return 0;
}
