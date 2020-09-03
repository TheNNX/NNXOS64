#ifndef NNX_ALLOC_HEADER
#define NNX_ALLOC_HEADER
#pragma pack(push, 1)

#include "nnxint.h"

typedef struct MemoryBlock {
	UINT64 size;
	struct MemoryBlock* next;
	UINT8 flags;
}MemoryBlock;

#pragma pack(pop)

#define MEMBLOCK_FREE 0
#define MEMBLOCK_USED 1

#ifdef __cplusplus
extern "C" {
#endif
	void NNXAllocatorInitialize();
	void NNXAllocatorAppend(void* memblock, UINT64 sizeofMemblock);

	void* NNXAllocatorAlloc(UINT64 size);
	void* NNXAllocatorAllocArray(UINT64 n, UINT64 size);
	void NNXAllocatorFree(void* address);
	void* NNXAllocatorAllocVerbose(UINT64 size);
#ifdef __cplusplus
}
#endif

#endif



