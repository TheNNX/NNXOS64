#ifndef NNX_PHYSALLOC_HEADER
#define NNX_PHYSALLOC_HEADER
#include "nnxint.h"

#ifdef __cplusplus
extern "C" {
#endif

	extern UINT8* GlobalPhysicalMemoryMap;
	extern UINT64 GlobalPhysicalMemoryMapSize;
	extern UINT64 MemorySize;

	void* InternalAllocatePhysicalPage();
	UINT8 InternalFreePhysicalPage(void*);
	void* InternalAllocatePhysicalPageEx(UINT8 type, UINT64 seekFromAddress, UINT64 seekToAddress);
	void* InternalAllocatePhysicalPageWithType(UINT8 type);

#define MEM_TYPE_USED 0
#define MEM_TYPE_FREE 1
#define MEM_TYPE_UTIL 2
#define MEM_TYPE_USED_PERM 3
#define MEM_TYPE_KERNEL 4
#define MEM_TYPE_OLD_KERNEL 5
#ifdef __cplusplus
}
#endif

#endif