#include "nnxalloc.h"
#include "../video/SimpleTextIO.h"
#include "MemoryOperations.h"

MemoryBlock* first;

void NNXAllocatorInitialize() {
	first = 0;
}

void NNXAllocatorAppend(void* memblock, UINT64 memblockSize) {
	if (!first) {
		first = memblock;
		first->size = memblockSize - sizeof(MemoryBlock);
		first->next = 0;
		first->flags = MEMBLOCK_FREE;
	}
	else {
		MemoryBlock* current = first;
		while (current->next) {
			current = current->next;
		}
		current->next = memblock;
		current->next->size = memblockSize - sizeof(MemoryBlock);
		current->next->next = 0;
		current->next->flags = MEMBLOCK_FREE;
	}
}

void* NNXAllocatorAlloc(UINT64 size) {
	
	if (size >= 4096) {
		PrintT("Searching for a free block of size = %i (impossible)\n", size);
		while (1);
	}
	MemoryBlock* current = first;
	
	while (current) {
		
		if (!(current->flags & MEMBLOCK_USED)) {
			if (current->size > size + sizeof(MemoryBlock)) {
				current->flags |= MEMBLOCK_USED;
				MemoryBlock* newBlock = (MemoryBlock*)(((UINT64)current) + size + sizeof(MemoryBlock));
				newBlock->next = current->next;
				current->next = newBlock;
				newBlock->size = current->size - (size + sizeof(MemoryBlock));
				current->size = size;
				newBlock->flags &= (~MEMBLOCK_USED);
				return (void*)(((UINT64)current) + sizeof(MemoryBlock));
			}
		}
		current = current->next;
	}
	PrintT("No free block found\n");
	return 0;
}

void* NNXAllocatorAllocArray(UINT64 n, UINT64 size) {
	void* result = NNXAllocatorAlloc(n*size);
	if(result)
		MemSet(result, 0, size*n);
	return result;
}

void NNXAllocatorFree(void* address) {
	MemoryBlock* toBeFreed = ((UINT64)address - sizeof(MemoryBlock));
	toBeFreed->flags &= (~MEMBLOCK_USED);
}