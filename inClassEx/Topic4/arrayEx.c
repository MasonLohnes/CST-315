#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PHYSICAL_MEMORY 2048

int phyMem[PHYSICAL_MEMORY];
int *virMem = NULL;
int vMemAmount = 0;
// 1 = unavailable, 0 = available

// Initialize Memory
void initializeMemory() {
	srand(time(NULL));

	vMemAmount = ((rand() % (600 - 100 + 1) + 100) / 8) * 8; // Requested memory is 100 - 300 elements long
	virMem = malloc(vMemAmount * sizeof(int));

	int i = 0;
	int value = 0;
	while (i < PHYSICAL_MEMORY) {
		value = 1 - value;
		int chunkSize = ((rand() % (400 - 100 + 1) + 100) / 8) * 8;

		for (int j = 0; j < chunkSize; j++) {
			if (i >= PHYSICAL_MEMORY) break;
			phyMem[i] = value;
			i++;
		}
	}
}

// Chunk Structure
typedef struct {
	int start;
	int length;
} MemoryChunk;

MemoryChunk availableChunks[PHYSICAL_MEMORY];
int chunkCount = 0;

// Find All Chunks
void scanMemChunks() {
	chunkCount = 0;
	int start = -1;
	int length = 0;

	for (int i = 0; i < PHYSICAL_MEMORY; i++) {
		if (phyMem[i] == 0) {
			if (start == -1) {
				start = i;
				length = 1;
			} else {
				length++;
			}
		} else {
			if (start != -1) {
				availableChunks[chunkCount].start = start;
				availableChunks[chunkCount].length = length;
				chunkCount++;
				start = -1;
				length = 0;
			}
		}
	}
	if (start != -1) {
		availableChunks[chunkCount].start = start;
		availableChunks[chunkCount].length = length;
		chunkCount++;
	}
}

// Print Memory Chunks
void printMemChunks() {
	printf("Available Chunks Table:\n");
	printf("%10s %10s\n", "Start", "Length");

	for (int i = 0; i < chunkCount; i++) {
		printf("%10d %10d\n", availableChunks[i].start, availableChunks[i].length);
	}
}

// Sort Chunks Biggest To Smallest
void sortAvailableChunks() {
	for (int i = 0; i < chunkCount - 1; i++) {
		for (int j = i + 1; j < chunkCount; j++) {
			if (availableChunks[j].length > availableChunks[i].length) {
				MemoryChunk temp = availableChunks[i];
				availableChunks[i] = availableChunks[j];
				availableChunks[j] = temp;
			}
		}
	}
}

// Process Chunk Structure
typedef struct {
	int start;
	int length;
} SubChunk;

#define MAX_SUBCHUNKS 10

typedef struct {
	int totalLength;
	int subChunkCount;
	SubChunk subChunks[MAX_SUBCHUNKS];
} ProcessChunk;

ProcessChunk processChunks[PHYSICAL_MEMORY];
int processCount = 0;

// Allocate Memory
int allocateMemory(int reqMem) {
	sortAvailableChunks();

	int totalAvailable = 0;
	for (int i = 0; i < chunkCount && totalAvailable < reqMem; i++) {
		totalAvailable += availableChunks[i].length;
	}
	if (totalAvailable < reqMem) {
		printf("\n\n❌ Not enough space to allocate %d units.\n", reqMem);
		return -1;
	}

	int remaining = reqMem;
	ProcessChunk newProcess;
	newProcess.totalLength = reqMem;
	newProcess.subChunkCount = 0;

	for (int i = 0; i < chunkCount && remaining > 0; i++) {
		int useStart = availableChunks[i].start;
		int useLen = (availableChunks[i].length < remaining) ? availableChunks[i].length : remaining;

		// Mark memory as used
		for (int j = useStart; j < useStart + useLen; j++) {
			phyMem[j] = 1;
		}

		// Track the subchunk
		newProcess.subChunks[newProcess.subChunkCount].start = useStart;
		newProcess.subChunks[newProcess.subChunkCount].length = useLen;
		newProcess.subChunkCount++;

		remaining -= useLen;
	}

	processChunks[processCount++] = newProcess;
	printf("✅ Allocated %d units across %d chunks (greedy)\n\n", reqMem, newProcess.subChunkCount);
	return 0;
}

void freeMemory(int processIndex) {
	if (processIndex < 0 || processIndex >= processCount) {
		printf("❌ Invalid process index: %d\n", processIndex);
		return;
	}

	ProcessChunk *proc = &processChunks[processIndex];

	for (int i = 0; i < proc->subChunkCount; i++) {
		int start = proc->subChunks[i].start;
		int len = proc->subChunks[i].length;

		for (int j = start; j < start + len; j++) {
			phyMem[j] = 0;
		}
	}

	printf("\n🗑️  Deallocated process %d (%d units across %d chunks)\n\n",
		processIndex, proc->totalLength, proc->subChunkCount);

	proc->totalLength = 0;
	proc->subChunkCount = 0;
}


int main() {
	initializeMemory();
	scanMemChunks();

	printMemChunks();

	printf("\nAmount of requested memory: %d\n\n", vMemAmount);

	allocateMemory(vMemAmount);
	scanMemChunks();
	printMemChunks();

	freeMemory(0);
	scanMemChunks();
	printMemChunks();

	free(virMem);
	return 0;
}
