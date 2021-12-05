#ifndef NNX_PAGING_HEADER
#define NNX_PAGING_HEADER
#include <nnxtype.h>
#include "../HAL/registers.h"

#define PML4EntryForRecursivePaging 510ULL

#ifdef __cplusplus
extern "C"
{
#endif

    VOID PagingInit(PBYTE PhysMemoryMap, QWORD PhysMemMapSize);
    VOID PagingKernelInit();

    /* Scans the current virtual address space for a free page and maps it to a given physical page */
    VOID* PagingAllocatePageWithPhysicalAddress(UINT64 min, UINT64 max, UINT16 flags, VOID* physPage);
    VOID* PagingAllocatePageWithPhysicalAddress2(UINT64 min, UINT64 max, UINT16 flags, VOID* physPage);
    VOID* PagingAllocatePageEx(UINT64 min, UINT64 max, UINT16 flags, UINT8 physMemType);
    VOID* PagingAllocatePage();
    VOID* PagingAllocatePageFromRange(UINT64 min, UINT64 max);
    VOID PagingMapPage(VOID* v, VOID* p, UINT16 f);
    VOID PagingTLBFlush();
    VOID PagingTLBFlushPage(UINT64 page);
    PVOID PagingAllocatePageBlockWithPhysicalAdresses(UINT64 n, UINT64 min, UINT64 max, UINT16 flags, PVOID physFirstPage);
    PVOID PagingAllocatePageBlock(UINT64 n, UINT16 flags);
    VOID PagingMapFramebuffer();

    inline UINT64 ToCanonicalAddress(UINT64 address)
    {
        return address | ((address & (1ULL << 47ULL)) ? (0xFFFF000000000000) : 0);
    }

#ifdef __cplusplus
}
#endif

#define PML4_COVERED_SIZE 0x1000000000000ULL
#define PDP_COVERED_SIZE 0x8000000000ULL
#define PD_COVERED_SIZE 0x40000000ULL
#define PT_COVERED_SIZE 0x200000ULL
#define PAGE_SIZE_SMALL 4096ULL

#define PAGE_ADDRESS_MASK 0xFFF
#define PAGE_PRESENT 1
#define PAGE_WRITE 2
#define PAGE_USER 4
#define PAGE_WRITE_THROUGH 8
#define PAGE_NO_CACHE 16
#define PAGE_ACCESSED 32
#define PAGE_DIRTY 64
#define PAGE_GLOBAL 256

#define PAGING_KERNEL_SPACE        0xFFFF800000000000ULL
#define PAGING_KERNEL_SPACE_END 0xFFFFFFFFFFFFFFFFULL
#define PAGING_USER_SPACE        0x0000000000000000ULL
#define PAGING_USER_SPACE_END    0x00007FFFFFFFFFFFULL

#define PAGE_SYSTEM 0
#define PAGE_READ 0
#define PAGE_NOT_PRESENT 0


#define KERNEL_INITIAL_ADDRESS 0x200000ULL
#define KERNEL_DESIRED_LOCATION 0xFFFFFF8000000000ULL
#define KERNEL_DESIRED_PML4_ENTRY (0xFFFFFF8000000000ULL >> (55ULL))

#define PAGE_ALIGN(x) (((UINT64)x)&(~PAGE_ADDRESS_MASK))
#endif