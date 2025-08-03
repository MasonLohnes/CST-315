// Mason Lohnes, CST-315, Simple Shell

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>
#include <ctype.h>
#include <dirent.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

// File Functions
// Function to create a file
void create_file(const char *path) {
    printf("Creating %s\n", path);
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        perror("Failed to create file");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "Initial content of the file.\n");
    fclose(file);
}

// Function to modify a file by appending content
void modify_file(const char *path) {
    printf("Modifying %s\n", path);
    FILE *file = fopen(path, "a");
    if (file == NULL) {
        perror("Failed to open file for modification");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "Appended content to the file.\n");
    fclose(file);
}

// Function to delete a file
void delete_file(const char *path) {
    printf("Deleting %s\n", path);
    if (remove(path) != 0) {
        perror("Failed to delete file");
        exit(EXIT_FAILURE);
    }
}

// Directory Functions
// Make Directory
void create_directory(const char *path) {
    if (mkdir(path, 0755) == 0) {
        printf("Directory '%s' created successfully.\n", path);
    } else {
        perror("newdir failed");
    }
    return;
}

// Delete Directory
void delete_directory(const char *path) {
    if (rmdir(path) == 0) {
        printf("Directory '%s' deleted successfully.\n", path);
    } else {
        perror("killdir failed");
    }
    return;
}

// Print Path
void printTree(const char *base_path, int depth) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(base_path);
    if (!dir) {
        perror("opendir failed");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Indent based on depth
        for (int i = 0; i < depth; i++)
            printf("│   ");
        printf("├── %s\n", entry->d_name);

        // Build full path
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);

        // If directory, recurse
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            printTree(path, depth + 1);
        }
    }

    closedir(dir);
}

// Rename Anything
void renameItem(const char *oldName, const char *newName) {
    if (rename(oldName, newName) == 0) {
        printf("Renamed '%s' to '%s'\n", oldName, newName);
    } else {
        perror("rename failed");
    }
    return;
}

void parse_args(char *input, char **args) {
    char *token;
    int i = 0;
    for (int j = 0; j < MAX_ARGS; j++) args[j] = NULL;
    token = strtok(input, " \t\r\n");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\r\n");
    }
}

void print_help() {
    printf("Simple Shell - built-in commands:\n");
    printf("  help       - Show this help message\n");
    printf("  cd [dir]   - Change current directory\n");
    printf("  exit       - Exit the shell\n");
    printf("  create [f] - Create file\n");
    printf("  modify [f] - Modify file\n");
    printf("  delete [f] - Delete file\n");
    printf("  newdir []  - Make new directory\n");
    printf("  killdir [] - Delete directory\n");
    printf(" rename [][] - Rename file or directory\n");
    printf(" path [dir]  - Show path tree\n");
}

void execute_command(char **args) {
    if (args[0] == NULL) return;

    if (strcmp(args[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    } else if (strcmp(args[0], "help") == 0) {
        print_help();
        return;
    } else if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            printf("Usage: cd <directory>\n");
        } else {
            if (chdir(args[1]) != 0) {
                perror("cd failed");
            }
        }
        return;
    } else if (strcmp(args[0], "create") == 0) {
        if (args[1] == NULL) {
            printf("No file-name given");
	    return;
        } else {
	    int i = 1;
	    while (args[i] != NULL) {
		create_file(args[i]);
		i++;
	    }
            return;
        }
    } else if (strcmp(args[0], "modify") == 0) {
        if (args[1] == NULL) {
            printf("No file-name given");
	    return;
        } else {
	    int i = 1;
	    while (args[i] != NULL) {
		modify_file(args[i]);
		i++;
	    }
            return;
        }
    } else if (strcmp(args[0], "delete") == 0) {
        if (args[1] == NULL) {
            printf("No file-name given");
	    return;
        } else {
	    int i = 1;
	    while (args[i] != NULL) {
		delete_file(args[i]);
		i++;
	    }
            return;
        }
    } else if (strcmp(args[0], "newdir") == 0) {
        if (args[1] == NULL) {
            printf("No file-name given");
	    return;
        } else {
	    int i = 1;
	    while (args[i] != NULL) {
		create_directory(args[i]);
		i++;
	    }
            return;
        }
    } else if (strcmp(args[0], "killdir") == 0) {
        if (args[1] == NULL) {
            printf("No file-name given");
	    return;
        } else {
	    int i = 1;
	    while (args[i] != NULL) {
		delete_directory(args[i]);
		i++;
	    }
            return;
        }
    } else if (strcmp(args[0], "rename") == 0) {
        if (args[1] == NULL || args[2] == NULL) {
            printf("Must give old then new name (rename oldName newName)");
	    return;
        } else {
            renameItem(args[1], args[2]);
            return;
        }
    } else if (strcmp(args[0], "tree") == 0) {
        if (args[1] == NULL) {
	    char cwd[PATH_MAX];
	    getcwd(cwd, sizeof(cwd));
            printTree(cwd, 0);
	    return;
        } else {
            printTree(args[1], 0);
            return;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
    } else if (pid == 0) {
        execvp(args[0], args);
        perror("Execution failed");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
}

int main() {
    char input[MAX_LINE];
    char *args[MAX_ARGS];

    while (1) {
	char cwd[PATH_MAX];
	getcwd(cwd, sizeof(cwd));

        printf("$simpleShell:%s$", cwd);
        if (fgets(input, sizeof(input), stdin) == NULL) break;

        parse_args(input, args);
        execute_command(args);
    }

    return 0;
}
