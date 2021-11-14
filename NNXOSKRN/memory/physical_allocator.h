#ifndef NNX_PHYSALLOC_HEADER
#define NNX_PHYSALLOC_HEADER
#include <nnxtype.h>

#ifdef __cplusplus
extern "C"
{
#endif

    PVOID InternalAllocatePhysicalPage();
    UINT8 InternalFreePhysicalPage(void*);
    PVOID InternalAllocatePhysicalPageEx(UINT8 type, UINT64 seekFromAddress, UINT64 seekToAddress);
    PVOID InternalAllocatePhysicalPageWithType(UINT8 type);
    
#define MEM_TYPE_USED 0
#define MEM_TYPE_FREE 1
#define MEM_TYPE_UTIL 2
#define MEM_TYPE_USED_PERM 3
#ifdef __cplusplus
}
#endif

#endif