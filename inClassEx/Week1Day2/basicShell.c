#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

void parse_input(char *input, char **args) {
    char *token;
    int i = 0;

    // Tokenize the input string
    token = strtok(input, " \t\r\n");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    args[i] = NULL;
}

void execute_command(char **args) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }

    if (pid == 0) {
        // Child process executes the command
        if (execvp(args[0], args) == -1) {
            perror("Execution failed");
        }
        exit(1);
    } else {
        // Parent waits for the child process to finish
        int status;
        waitpid(pid, &status, 0);
    }
}

int main() {
    char input[MAX_LINE];
    char *args[MAX_ARGS];

    while (1) {
        // Display prompt
        printf("my_shell> ");
        fflush(stdout);

        // Get input from the user
        if (fgets(input, MAX_LINE, stdin) == NULL) {
            perror("Input error");
            continue;
        }

        // Exit if input is "buzz off"
        if (strncmp(input, "buzz off", 4) == 0) {
            break;
        }

        // Parse input into arguments
        parse_input(input, args);

        // Ignore empty input
        if (args[0] == NULL) {
            continue;
        }

	// Change directory if user says jump
	if (strcmp(args[0], "jump") == 0) {
	    if (args[1] == NULL){
		    printf("No directory given, :(\n");
		}
	    if (args[2] != NULL){
		    printf("Too many arguments, :(\n");
		}
	    else {
		if (chdir(args[1]) != 0) {
    		    perror("chdir failed");
		}
	    	else {
    		printf("Changed directory to %s\n", args[1]);
		}
	    }
	    continue;
	}

        // Execute the command
        execute_command(args);
    }

    return 0;
}
