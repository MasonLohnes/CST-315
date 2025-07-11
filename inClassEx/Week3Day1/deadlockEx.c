#include <stdio.h>
#include <stdbool.h>

#define NUM_PROCESSES 4

// Graph represented as an adjacency matrix
int waitForGraph[NUM_PROCESSES][NUM_PROCESSES] = {
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1},
    {1, 0, 0, 0}
};

bool visited[NUM_PROCESSES];
bool recStack[NUM_PROCESSES];

bool isCyclicUtil(int process) {
    if (!visited[process]) {
        visited[process] = true;
        recStack[process] = true;

        for (int adj = 0; adj < NUM_PROCESSES; adj++) {
            if (waitForGraph[process][adj]) {
                if (!visited[adj] && isCyclicUtil(adj))
                    return true;
                else if (recStack[adj])
                    return true;
            }
        }
    }
    recStack[process] = false;
    return false;
}

bool isCyclic() {
    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (isCyclicUtil(i))
            return true;
    }
    return false;
}

int main() {
    for (int i = 0; i < NUM_PROCESSES; i++) {
        visited[i] = false;
        recStack[i] = false;
    }

    if (isCyclic())
        printf("Deadlock detected!\n");
    else
        printf("No Deadlock.\n");

    return 0;
}
