#include "nnxalloc.h"
#include "../video/SimpleTextIO.h"
#include "MemoryOperations.h"

MemoryBlock* first;
BOOL dirty = false;


void NNXAllocatorInitialize() {
	first = 0;
}

void NNXAllocatorAppend(void* memblock, UINT64 memblockSize) {
	if (!first) {
		first = memblock;
		first->size = memblockSize - sizeof(MemoryBlock) - 1;
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
	dirty = true;
}

void TryMerge() {
	PrintT("System memory allocation utility is attempting to merge free memory blocks\n");
	MemoryBlock* current = first;

	if (current == 0)
		return 0;

	dirty = false;

	while (current->next) {

		if (!(current->flags & MEMBLOCK_USED) && !(current->next->flags & MEMBLOCK_USED) &&
			(((UINT64)current) + current->size + sizeof(MemoryBlock)) == current->next) {
			current->size += current->next->size + sizeof(MemoryBlock);
			current->next = current->next->next;
		}
		else {
			current = current->next;
		}
		
	}
}

void* NNXAllocatorAllocH(UINT64 size, UINT8 verbose) {
	
	MemoryBlock* current = first;
	
	while (current) {

		if (current->flags & (~MEMBLOCK_USED)) {
			PrintT("Invalid block of size %x at address %x leading to address %x (flags %b) in kernel allocator\n", current->size, current, current->next, current->flags);
			while (1);
		}

		if (!(current->flags & MEMBLOCK_USED)) {
			
			if (current->size == size + sizeof(MemoryBlock)) {
				current->flags |= MEMBLOCK_USED;
				dirty = true;
				return (void*)(((UINT64)current) + sizeof(MemoryBlock));
			}
			else if (current->size > size + sizeof(MemoryBlock)) {
				current->flags |= MEMBLOCK_USED;
				MemoryBlock* newBlock = (MemoryBlock*)((((UINT64)current) + size + sizeof(MemoryBlock)));
				newBlock->next = current->next;
				current->next = newBlock;
				newBlock->size = current->size - (size + sizeof(MemoryBlock));
				current->size = size;
				newBlock->flags = MEMBLOCK_FREE;
				dirty = true;
				return (void*)(((UINT64)current) + sizeof(MemoryBlock));
			}
		}
		current = current->next;
	}
	
	if (dirty) {
		TryMerge();
		return NNXAllocatorAllocH(size, verbose);
	}
	PrintT("No memory block of size %iB found, system halted.\n", size);
	while (1);
}

void* NNXAllocatorAlloc(UINT64 size) {
	return NNXAllocatorAllocH(size, 0);
}

void* NNXAllocatorAllocVerbose(UINT64 size) {
	return NNXAllocatorAllocH(size, 1);
}

void* NNXAllocatorAllocArray(UINT64 n, UINT64 size) {
	void* result = NNXAllocatorAlloc(n*size);
	if(result)
		MemSet(result, 0, size*n);
	return result;
}

void NNXAllocatorFree(void* address) {
	dirty = true;
	MemoryBlock* toBeFreed = ((UINT64)address - sizeof(MemoryBlock));
	toBeFreed->flags &= (~MEMBLOCK_USED);
}