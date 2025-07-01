#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    for (int i = 0; i < 4; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork failed");
            return 1;
        } else if (pid == 0) {
            // Child process
            printf("Child process, PID = %d\n", getpid());
            if (i == 0) execlp("ls", "ls", NULL);
            else if (i == 1) execlp("ps", "ps", NULL);
            else if (i == 2) execlp("date", "date", NULL);
            else if (i == 3) execlp("whoami", "whoami", NULL);

            perror("execlp failed");
            return 1;
        }
    }

    // Parent process
    printf("Parent Process, PID = %d\n", getpid());

    for (int j = 0; j < 4; j++) {
        int status;
        pid_t child_pid = wait(&status);
        if (child_pid > 0) {
            printf("Parent: Child with PID %d has finished.\n", child_pid);
        }
    }

    printf("All children have finished executing.\n");

    return 0;
}
