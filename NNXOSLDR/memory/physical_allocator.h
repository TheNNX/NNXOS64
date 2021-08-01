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

#ifdef __cplusplus
}
#endif

#endif