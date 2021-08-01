#ifndef NNX_PAGING_HEADER
#define NNX_PAGING_HEADER
#include "nnxint.h"

#ifdef __cplusplus
extern "C" {
#endif
	extern UINT64**** PML4;
	extern UINT64**** PML4_IdentifyMap;

	UINT64**** GetCR3();
	void SetCR3(UINT64****);
	void PagingTest();
	void PagingInit();
	void* PagingAllocatePage();
	void PagingMapPage(UINT64 v, UINT64 p, UINT16 f);

#ifdef __cplusplus
}
#endif

#define PML4_ADDRESS_MASK 0xff8000000000
#define PDP_ADDRESS_MASK 0x7fc0000000
#define PD_ADDRESS_MASK 0x3fe00000
#define PT_ADDRESS_MASK 0x1ff000
#define PAGE_ADDRESS_MASK 0xfff
#define PAGE_PRESENT 1
#define PAGE_WRITE 2
#define PAGE_USER 4
#define PAGE_SYSTEM 0
#define PAGE_READ 0
#define PAGE_NOT_PRESENT 0
#define PG_ALIGN(x) (((UINT64)x)&(~PAGE_ADDRESS_MASK))
#endif