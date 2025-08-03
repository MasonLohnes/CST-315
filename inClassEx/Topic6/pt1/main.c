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
#include <errno.h>
#include <libgen.h>
#include <time.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

// File Functions
// Function to create a file
void create_file(const char *path, int random_size) {
    size_t size = random_size ?
        (rand() % (10 * 1024 * 1024 - 1024)) + 1024 :  // Random 1KB-10MB
        1024;                                           // Default 1KB

    printf("Creating %s (%s%zu bytes)\n",
           path, random_size ? "random " : "", size);

    FILE *file = fopen(path, "w");
    if (!file) {
        perror("Failed to create file");
        exit(EXIT_FAILURE);
    }

    unsigned char *data = malloc(size);
    if (!data) {
        perror("Memory allocation failed");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }

    fwrite(data, 1, size, file);
    free(data);
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

// Display File Info
void get_file_info(const char *path, int detailed) {
    struct stat st;
    if (stat(path, &st) != 0) {
        perror("Error getting file info");
        return;
    }

    printf("\nInformation for: %s\n", path);
    printf("Type: Regular File\n");
    printf("Size: %ld bytes\n", st.st_size);
    printf("Permissions: %o\n", st.st_mode & 0777);
    printf("Last modified: %s", ctime(&st.st_mtime));

    if (detailed) {
        printf("\n-- Detailed Information --\n");
        printf("Inode: %ld\n", st.st_ino);
        printf("Hard Links: %ld\n", st.st_nlink);
        printf("Owner UID: %d\n", st.st_uid);
        printf("Group GID: %d\n", st.st_gid);
        printf("Device: %ld\n", st.st_dev);
        printf("Last access: %s", ctime(&st.st_atime));
        printf("Last status change: %s", ctime(&st.st_ctime));
    }
    printf("\n");
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

// Delete Contents of Directory
void delete_directory_contents(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("Failed to open directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full path
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            // Recursively delete subdirectory
            delete_directory_contents(full_path);
            rmdir(full_path);
        } else {
            // Delete file
            unlink(full_path);
        }
    }
    closedir(dir);
}

// Delete Directory
void delete_directory(const char *path, int recursive) {
    if (recursive) {
        delete_directory_contents(path);
    }

    if (rmdir(path) == 0) {
        printf("Directory '%s' deleted successfully.\n", path);
    } else {
        if (recursive) {
            perror("Failed to delete directory (even with -r)");
        } else {
            printf("Directory not empty. Use -r to delete recursively.\n");
        }
    }
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

// Display Directory Info
void get_dir_info(const char *path, int detailed) {
    struct stat st;
    if (stat(path, &st) != 0) {
        perror("Error getting directory info");
        return;
    }

    printf("\nInformation for: %s\n", path);
    printf("Type: Directory\n");
    printf("Permissions: %o\n", st.st_mode & 0777);
    printf("Last modified: %s", ctime(&st.st_mtime));

    if (detailed) {
        DIR *dir = opendir(path);
        int file_count = 0;
        int dir_count = 0;
        long total_size = 0;
        struct dirent *entry;

        printf("\n-- Detailed Information --\n");
        printf("Inode: %ld\n", st.st_ino);
        printf("Hard Links: %ld\n", st.st_nlink);
        printf("Owner UID: %d\n", st.st_uid);
        printf("Group GID: %d\n", st.st_gid);
        printf("Device: %ld\n", st.st_dev);

        if (dir) {
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;

                char full_path[PATH_MAX];
                snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

                struct stat entry_st;
                if (stat(full_path, &entry_st) == 0) {
                    if (S_ISDIR(entry_st.st_mode)) {
                        dir_count++;
                    } else {
                        file_count++;
                        total_size += entry_st.st_size;
                    }
                }
            }
            closedir(dir);

            printf("Contents: %d files, %d subdirectories\n", file_count, dir_count);
            printf("Total size of files: %ld bytes\n", total_size);
        }
        printf("Last access: %s", ctime(&st.st_atime));
        printf("Last status change: %s", ctime(&st.st_ctime));
    }
    printf("\n");
}

// Functions for Both
// Rename Anything
void renameItem(const char *oldName, const char *newName) {
    if (rename(oldName, newName) == 0) {
        printf("Renamed '%s' to '%s'\n", oldName, newName);
    } else {
        perror("rename failed");
    }
    return;
}

// Move Anything
void moveItem(const char *item, const char *newPath) {
    const char *filename = strrchr(item, '/');
    filename = (filename == NULL) ? item : filename + 1;

    char fullDestPath[1024];  // Adjust buffer size as needed
    snprintf(fullDestPath, sizeof(fullDestPath), "%s/%s", newPath, filename);

    if (rename(item, fullDestPath) == 0) {
        printf("Moved '%s' to '%s'\n", item, fullDestPath);
    } else {
        perror("move failed");
    }
    return;
}

// Returns Base Filename
const char *get_basename(const char *path) {
    const char *last_slash = strrchr(path, '/');
    return last_slash ? last_slash + 1 : path;
}

// Copies Directories or File with Number Formatting
void auto_copy(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        perror("Error checking path");
        return;
    }

    char newname[256];
    char command[512];
    int counter = 1;

    const char *base = get_basename(path);
    const char *dot = strrchr(base, '.');

    do {
        if (dot && S_ISREG(st.st_mode)) {
            snprintf(newname, sizeof(newname), "%.*s(%d)%s",
                    (int)(dot - base), base, counter, dot);
        } else {
            snprintf(newname, sizeof(newname), "%s(%d)", base, counter);
        }
        counter++;
    } while (access(newname, F_OK) == 0);

    if (S_ISDIR(st.st_mode)) {
        snprintf(command, sizeof(command), "cp -r \"%s\" \"%s\"", path, newname);
    } else {
        snprintf(command, sizeof(command), "cp \"%s\" \"%s\"", path, newname);
    }

    system(command);
    printf("Created copy: %s\n", newname);
}

// Directory Search
int search_file(const char *dir_path, const char *target_file, int *found) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        // Don't print error for directories we can't access
        if (errno != EACCES) {
            perror("Failed to open directory");
        }
        return 0;
    }

    struct dirent *entry;
    char path[PATH_MAX];
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        if (strcmp(entry->d_name, target_file) == 0) {
            printf("Found: %s\n", path);
            (*found)++;
            count++;
        }

        if (entry->d_type == DT_DIR) {
            count += search_file(path, target_file, found);
        }
    }
    closedir(dir);
    return count;
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
    printf("  help               - Show this help message\n");
    printf("  cd [dir]           - Change current directory\n");
    printf("  exit               - Exit the shell\n");
    printf("  create [-f] <file> - Create file (use -f for random size)\n");
    printf("  modify <file>      - Modify file\n");
    printf("  delete <file>      - Delete file\n");
    printf("  newdir <dir>       - Make new directory\n");
    printf("  killdir [-r] <dir> - Delete directory (use -r for recursive)\n");
    printf("  rename <old> <new> - Rename file or directory\n");
    printf("  tree [dir]         - Show directory tree\n");
    printf("  move <src> <dest>  - Move file/directory\n");
    printf("  copy <target>      - Copy file/directory\n");
    printf("  search <file>      - Search for file in directory tree\n");
    printf("  fileinfo [-d] <file> - Get file information (use -d for details)\n");
    printf("  dirinfo [-d] <dir>   - Get directory info (use -d for details)\n");
    printf("\nUse 'man <command>' for more information about specific commands.\n");
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
            printf("Usage: create [-f] <file1> [file2...]\n");
            return;
        }

        int random_size = 0;
        int file_arg_start = 1;

        // Check for -f flag
        if (strcmp(args[1], "-f") == 0) {
            if (args[2] == NULL) {
                printf("Error: -f requires filename\n");
                return;
            }
            random_size = 1;
            file_arg_start = 2;
        }

        // Initialize random seed once
        static int seeded = 0;
        if (!seeded) {
            srand(time(NULL));
        seeded = 1;
        }

        // Process files
        for (int i = file_arg_start; args[i] != NULL; i++) {
            create_file(args[i], random_size);
        }
	return;
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
            printf("Usage: killdir [-r] <directory>...\n");
            return;
        }

        int recursive = 0;
        int i = 1;

        // Check for -r flag
        if (strcmp(args[1], "-r") == 0) {
            recursive = 1;
            i = 2;
            if (args[i] == NULL) {
                printf("No directories specified after -r\n");
                return;
            }
        }

        while (args[i] != NULL) {
            delete_directory(args[i], recursive);
            i++;
        }
	return;
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
    } else if (strcmp(args[0], "move") == 0) {
        if (args[1] == NULL || args[2] == NULL) {
            printf("Must give old then new name (move file newDirectory)");
	    return;
        } else {
            moveItem(args[1], args[2]);
            return;
        }
    } else if (strcmp(args[0], "copy") == 0) {
        if (args[1] == NULL) {
            printf("Usage: copy <file_or_directory>\n");
            return;
        }

        // Handle multiple arguments (copy file1 file2 file3)
        int i = 1;
        while (args[i] != NULL) {
            auto_copy(args[i]);
            i++;
        }
        return;
    } else if (strcmp(args[0], "search") == 0) {
        if (args[1] == NULL) {
            printf("Usage: search <filename>\n");
            return;
        }

        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("Failed to get current directory");
            return;
        }

        printf("Searching for '%s' in %s and subdirectories...\n", args[1], cwd);

        int found = 0;
        int total_searched = search_file(cwd, args[1], &found);

        if (found == 0) {
            printf("No matches found for '%s'\n", args[1]);
        } else {
            printf("Found %d match(es)\n", found);
        }
        return;
    } else if (strcmp(args[0], "finf") == 0) {
        if (args[1] == NULL) {
            printf("Usage: fileinfo [-d] <file>\n");
            return;
        }

        int detailed = 0;
        const char *target = args[1];

        if (strcmp(args[1], "-d") == 0) {
            if (args[2] == NULL) {
                printf("Error: No file specified after -d\n");
                return;
            }
            detailed = 1;
            target = args[2];
        }

        get_file_info(target, detailed);
        return;
    } else if (strcmp(args[0], "dinf") == 0) {
        if (args[1] == NULL) {
            printf("Usage: dirinfo [-d] <directory>\n");
            return;
        }

        int detailed = 0;
        const char *target = args[1];

        if (strcmp(args[1], "-d") == 0) {
            if (args[2] == NULL) {
                printf("Error: No directory specified after -d\n");
                return;
            }
            detailed = 1;
            target = args[2];
        }
        get_dir_info(target, detailed);
        return;
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

        printf("$simpleShell:%s$ ", cwd);
        if (fgets(input, sizeof(input), stdin) == NULL) break;

        parse_args(input, args);
        execute_command(args);
    }

    return 0;
}
