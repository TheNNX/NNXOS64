#ifndef NNX_PAGING_HEADER
#define NNX_PAGING_HEADER
#include "nnxint.h"
#include "../HAL/registers.h"

void PrintTA(const char* i, ...);

#ifdef __cplusplus
extern "C" {
#endif

	void PagingTest();
	void PagingInit();
	void PagingKernelInit();
	void* PagingAllocatePage();
	void* PagingAllocatePageFromRange(UINT64 min, UINT64 max);
	void PagingMapPage(UINT64 v, UINT64 p, UINT16 f);

	void PagingTLBFlush();
	void PagingTLBFlushPage(UINT64 page);

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
#define PAGE_SIZE 4096ULL

#define PAGE_ADDRESS_MASK 0xFFF
#define PAGE_PRESENT 1
#define PAGE_WRITE 2
#define PAGE_USER 4
#define PAGE_GLOBAL 256
#define PAGE_SYSTEM 0
#define PAGE_READ 0
#define PAGE_NOT_PRESENT 0
#define PAGE_ALIGN(x) (((UINT64)x)&(~PAGE_ADDRESS_MASK))
#endif