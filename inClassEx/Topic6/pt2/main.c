#include <stdio.h>

#define STORAGE_SIZE 1000
#define MAX_FILES 100
#define FILENAME_LEN 20

typedef struct {
	int start;
	int length;
} Chunk;

typedef struct {
	char name[FILENAME_LEN];
	int size;
	int chunkCount;
	Chunk chunks[10];  // Supports up to 10 fragments per file
	int active;        // 1 if file exists, 0 if deleted
} FileEntry;

char storage[STORAGE_SIZE];
FileEntry fileTable[MAX_FILES];
int fileCount = 0;

void initializeFileStorage () {
	
}

int main () {
	printf("test");
	return 0;
}
