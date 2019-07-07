#pragma once
#pragma pack(push, 1)
#include <malloc.h>
#include "nnxint.h"

typedef struct MemoryBlock {
	UINT64 size;
	struct MemoryBlock* next;
	UINT8 flags;
}MemoryBlock;

#define MEMBLOCK_FREE 0
#define MEMBLOCK_USED 1

void nnxalloc_init();
void nnxalloc_append(void* memblock, UINT64 sizeofMemblock);

void* nnxmalloc(UINT64 size);
void* nnxcalloc(UINT64 n, UINT64 size);
void nnxfree(void* address);

#pragma pack(pop)

