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

	UINT64 NNXAllocatorGetTotalMemory();
	UINT64 NNXAllocatorGetUsedMemory();
	UINT64 NNXAllocatorGetFreeMemory();
	UINT64 NNXAllocatorGetUsedMemoryInBlocks();
#ifdef __cplusplus
}
#endif


#ifdef DEBUG

#define SaveStateOfMemory(c)\
		__caller = c;\
		__lastMemory = NNXAllocatorGetUsedMemoryInBlocks()

#define CheckMemory()\
		__currentMemory = NNXAllocatorGetUsedMemoryInBlocks();\
		if (__lastMemory < __currentMemory) {\
			PrintT("----------------\n");\
			if (__caller)\
				PrintT("%s: Potential memory leak of %i bytes\n", __caller, __currentMemory - __lastMemory);\
			PrintT("Total memory: %i, Used memory: %i, Free memory: %i\n", NNXAllocatorGetTotalMemory(), NNXAllocatorGetUsedMemory(), NNXAllocatorGetFreeMemory());\
		}\
		else if (__lastMemory > __currentMemory) {\
			PrintT("----------------\n");\
			if (__caller)\
				PrintT("%s: Somehow we have more memory.\n", __caller);\
			PrintT("Investigate... %i\n", __lastMemory - __currentMemory);\
		}\
		__caller = 0
#else 
#define SaveStateOfMemory(c)
#define CheckMemory()

#endif

#endif



