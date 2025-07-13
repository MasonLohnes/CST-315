#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <termios.h>
#include <signal.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_COMMANDS 10

struct termios original_term;
pid_t foreground_pgid = 0;
volatile sig_atomic_t ctrl_x_pressed = 0;
int is_interactive = 0;

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
                        if (strcmp(args[0], "quit") == 0) {
                                printf("Exiting shell...\n");
                                exit(0);
                        }

                        pids[i] = fork();

                        if (pids[i] < 0) {
                                perror("Fork failed");
                                continue;
                        }

                        if (pids[i] == 0) {
                                setpgid(0, 0);
                                signal(SIGINT, SIG_DFL);
                                execvp(args[0], args);
                                perror("Execution failed");
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
                        if (ctrl_x_pressed) {
                                printf("\nExiting shell...\n");
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

                // Echo the command to simulate shell behavior
                printf("%s\n", line);

                memset(commands, 0, sizeof(commands));
                parse_commands(line, commands);
                if (commands[0] != NULL) {
                        execute_commands(commands);
                }
        }

        fclose(file);
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
                                exit(0);
                        }

                        if (c == '\n') {
                                input[pos] = '\0';
                                putchar('\n');
                                break;
                        }
                        else if (c == 24) {  // Ctrl+X
                                printf("\nExiting shell...\n");
                                exit(0);
                        }
                        else if (c == 127) {  // Backspace
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
                        exit(0);
                }

                if (pos == 0) continue;

                if (strcmp(input, "quit") == 0) {
                        printf("Exiting shell...\n");
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
        if (argc > 1) {
                is_interactive = 0;
                process_batch_file(argv[1]);
        } else {
                is_interactive = 1;
                interactive_mode();
        }
        return 0;
}
