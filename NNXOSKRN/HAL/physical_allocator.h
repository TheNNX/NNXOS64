#ifndef NNX_PHYSALLOC_HEADER
#define NNX_PHYSALLOC_HEADER
#include <nnxtype.h>

#ifdef __cplusplus
extern "C"
{
#endif

    ULONG_PTR InternalAllocatePhysicalPage();
    UINT8 InternalFreePhysicalPage(ULONG_PTR);
    ULONG_PTR InternalAllocatePhysicalPageEx(UINT8 type, ULONG_PTR seekFromAddress, ULONG_PTR seekToAddress);
    ULONG_PTR InternalAllocatePhysicalPageWithType(UINT8 type);
    BYTE InternalMarkPhysPage(UINT8 type, ULONG_PTR PhysPage);
    
#define MEM_TYPE_USED 0
#define MEM_TYPE_FREE 1
#define MEM_TYPE_UTIL 2
#define MEM_TYPE_USED_PERM 3
#ifdef __cplusplus
}
#endif

#endif