#ifndef NNX_PHYSALLOC_HEADER
#define NNX_PHYSALLOC_HEADER
#include "nnxint.h"
extern UINT8* GlobalPhysicalMemoryMap;
extern UINT64 GlobalPhysicalMemoryMapSize;
extern UINT64 MemorySize;

void* InternalAllocatePhysicalPage();
UINT8 InternalFreePhysicalPage(void*);
#endif