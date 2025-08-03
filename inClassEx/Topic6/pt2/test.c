#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STORAGE_SIZE 100
#define MAX_FILES 20
#define MAX_CHUNKS 10
#define FILENAME_LEN 20

typedef struct {
    int start;
    int length;
} Chunk;

typedef struct {
    char name[FILENAME_LEN];
    int size;
    int chunkCount;
    Chunk chunks[MAX_CHUNKS];
    int active;
} FileEntry;

char storage[STORAGE_SIZE];
FileEntry fileTable[MAX_FILES];
int fileCount = 0;

// Declring All Functions Early
void initializeFilesStorage();
void visualizeStorage();
void newFile(const char* fileName, int fileSize);
void saveFile(const char* fileName);
void deleteFile(const char* fileName);
void storageStatus();
void defrag();
int findFreeBlock(int needed, int* start);

// Main
int main() {
    initializeFilesStorage();
    visualizeStorage();
    storageStatus();

    printf("\n>>> Deleting init1 to create fragmentation\n");
    deleteFile("init1");
    visualizeStorage();
    storageStatus();

    printf("\n>>> Saving file3 (size 30), likely to be fragmented\n");
    newFile("file3", 30);
    saveFile("file3");
    visualizeStorage();
    storageStatus();

    printf("\n>>> Running defragmentation\n");
    defrag();
    visualizeStorage();
    storageStatus();

    return 0;
}

// Example file initialization
void initializeFilesStorage() {
    memset(storage, '.', sizeof(storage));
    memset(fileTable, 0, sizeof(fileTable));
    fileCount = 0;

    newFile("init1", 20); saveFile("init1");
    newFile("init2", 25); saveFile("init2");
    newFile("init3", 15); saveFile("init3");
}

// Output line of storage
void visualizeStorage() {
    printf("\nStorage: [size = %d]\n", STORAGE_SIZE);
    for (int i = 0; i < STORAGE_SIZE; i++) {
        printf("%c", storage[i]);
    }
    printf("\n");
}

// Create empty file
void newFile(const char* fileName, int fileSize) {
    if (fileCount >= MAX_FILES) {
        printf("File table full.\n");
        return;
    }

    FileEntry* file = &fileTable[fileCount++];
    strncpy(file->name, fileName, FILENAME_LEN);
    file->size = fileSize;
    file->chunkCount = 0;
    file->active = 1;
}

// Save file to storage with possuble fragmentation
void saveFile(const char* fileName) {
    for (int i = 0; i < fileCount; i++) {
        if (fileTable[i].active && strcmp(fileTable[i].name, fileName) == 0) {
            int remaining = fileTable[i].size;
            int pos = 0;
            while (remaining > 0 && fileTable[i].chunkCount < MAX_CHUNKS) {
                int start, block;
                block = findFreeBlock(remaining, &start);
                if (block == 0) break;  // No space left

                // Allocate this block
                fileTable[i].chunks[fileTable[i].chunkCount].start = start;
                fileTable[i].chunks[fileTable[i].chunkCount].length = block;
                fileTable[i].chunkCount++;

                for (int j = start; j < start + block; j++) {
                    storage[j] = (i + 1 < 10) ? '0' + (i + 1) : 'A' + ((i + 1) - 10);
                }

                remaining -= block;
            }

            if (remaining > 0) {
                printf("Not enough space to save %s completely.\n", fileName);
            } else {
                printf("Saved file %s (possibly fragmented).\n", fileName);
            }
            return;
        }
    }
    printf("File not found: %s\n", fileName);
}

// Find next free block
int findFreeBlock(int needed, int* start) {
    int i = 0;
    while (i < STORAGE_SIZE) {
        while (i < STORAGE_SIZE && storage[i] != '.') i++;
        int s = i;
        while (i < STORAGE_SIZE && storage[i] == '.') i++;
        int freeLen = i - s;
        if (freeLen > 0) {
            *start = s;
            return (freeLen < needed) ? freeLen : needed;
        }
    }
    return 0;
}

// Delete a file
void deleteFile(const char* fileName) {
    for (int i = 0; i < fileCount; i++) {
        if (fileTable[i].active && strcmp(fileTable[i].name, fileName) == 0) {
            for (int j = 0; j < fileTable[i].chunkCount; j++) {
                int start = fileTable[i].chunks[j].start;
                int len = fileTable[i].chunks[j].length;
                for (int k = start; k < start + len; k++) {
                    storage[k] = '.';
                }
            }
            fileTable[i].active = 0;
            printf("Deleted file: %s\n", fileName);
            return;
        }
    }
    printf("File not found: %s\n", fileName);
}

// Print out the amount of fragments, and the amount of used storage
void storageStatus() {
    int used = 0, fragments = 0;
    for (int i = 0; i < STORAGE_SIZE; i++) {
        if (storage[i] != '.') used++;
        if (i > 0 && storage[i] != '.' && storage[i-1] == '.') fragments++;
    }
    printf("\nStorage Status:\n");
    printf("Used: %d / %d\n", used, STORAGE_SIZE);
    printf("Free: %d\n", STORAGE_SIZE - used);
    printf("Fragments: %d\n", fragments);
}

// Defragment
void defrag() {
    char newStorage[STORAGE_SIZE];
    memset(newStorage, '.', STORAGE_SIZE);
    int current = 0;

    for (int i = 0; i < fileCount; i++) {
        if (!fileTable[i].active) continue;

        int remaining = fileTable[i].size;
        fileTable[i].chunkCount = 0;

        while (remaining > 0) {
            int block = (remaining < STORAGE_SIZE - current) ? remaining : (STORAGE_SIZE - current);
            fileTable[i].chunks[fileTable[i].chunkCount].start = current;
            fileTable[i].chunks[fileTable[i].chunkCount].length = block;
            fileTable[i].chunkCount++;

            for (int j = 0; j < block; j++) {
                newStorage[current + j] = (i + 1 < 10) ? '0' + (i + 1) : 'A' + ((i + 1) - 10);
            }

            current += block;
            remaining -= block;
        }
    }

    memcpy(storage, newStorage, STORAGE_SIZE);
    printf("Defragmentation complete.\n");
}
