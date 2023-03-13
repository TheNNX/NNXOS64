#ifndef NNX_ALLOC_HEADER
#define NNX_ALLOC_HEADER

#include <pool.h>

#ifndef NNX_ALLOC_DEBUG
#define NNX_ALLOC_DEBUG 0
#endif
#define NNXAllocatorAlloc(x) ExAllocatePool(NonPagedPool, x)
#define NNXAllocatorFree(x) ExFreePool(x)

#endif



