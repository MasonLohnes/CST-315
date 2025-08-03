// Mason Lohnes, CST-315, Assignment 4: Injection Virus
// Malware detection program that scans a directory for suspicious command injection patterns

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <unistd.h>

bool is_infected(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (!file) return false;

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "rm -rf *") || strstr(line, "system(\"rm -rf *\")") || strstr(line, "; rm -rf *")) {
            fclose(file);
            return true;
        }
    }
    fclose(file);
    return false;
}

void scan_directory(const char *dirname) {
    DIR *dir = opendir(dirname);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    char filepath[512];

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // Regular file
            if (strcmp(entry->d_name, "detector.c") == 0) continue;
            snprintf(filepath, sizeof(filepath), "%s/%s", dirname, entry->d_name);
            if (is_infected(filepath)) {
                printf("Warning: file %s is infected with potential command injection!\n", filepath);
            }
        }
    }
    closedir(dir);
}

int main() {
    printf("\n==== Scanning for Infections in Current Directory ====\n");
    scan_directory(".");
    return 0;
}
