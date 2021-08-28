#ifndef NNX_PAGING_HEADER
#define NNX_PAGING_HEADER
#include "nnxint.h"
#include "../HAL/registers.h"

#ifdef __cplusplus
extern "C" {
#endif

	VOID PagingInit();
	VOID PagingKernelInit();

	/* Scans the current virtual address space for a free page and maps it to a given physical page */
	VOID* PagingAllocatePageWithPhysicalAddress(UINT64 min, UINT64 max, UINT16 flags, VOID* physPage);
	VOID* PagingAllocatePageEx(UINT64 min, UINT64 max, UINT16 flags, UINT8 physMemType);
	VOID* PagingAllocatePage();
	VOID* PagingAllocatePageFromRange(UINT64 min, UINT64 max);
	VOID PagingMapPage(VOID* v, VOID* p, UINT16 f);
	VOID PagingTLBFlush();
	VOID PagingTLBFlushPage(UINT64 page);

	inline UINT64 ToCanonicalAddress(UINT64 address) {
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

#define PAGE_SYSTEM 0
#define PAGE_READ 0
#define PAGE_NOT_PRESENT 0

#define PAGE_ALIGN(x) (((UINT64)x)&(~PAGE_ADDRESS_MASK))
#endif