#include "paging.h"
#include "video/SimpleTextIO.h"
#include "physical_allocator.h"
#include "MemoryOperations.h"

__declspec(align(4096)) UINT64**** PML4;
__declspec(align(4096)) UINT64*** PML4e[512];
__declspec(align(4096)) UINT64* temp[512] = { 0 };
__declspec(align(4096)) UINT64 temp2[512] = { 0 };

UINT64**** PML4_identify_map;
UINT64*** PDP_identify_map;

void PagingMapPage(void* virtual, void* physical, UINT16 flags) {

	//WIP

	UINT64**** pml4 = GetCR3();
	UINT64 pdp_number = (((UINT64)virtual) & PML4_ADDRESS_MASK) >> 39;
	UINT64 pd_number = (((UINT64)virtual) & PDP_ADDRESS_MASK) >> 30;
	UINT64 pt_number = (((UINT64)virtual) & PD_ADDRESS_MASK) >> 21;
	UINT64 page_number = (((UINT64)virtual) & PT_ADDRESS_MASK) >> 12;

	if (pml4[pdp_number] == 0) {
		pml4[pdp_number] = ((UINT64)InternalAllocatePhysicalPage()) | 7;
		memset(PG_ALIGN(pml4[pdp_number]), 0, 4096);
	}
	UINT64*** pdp = PG_ALIGN(PML4[pdp_number]);
	if (pdp[pd_number] == 0) {
		pdp[pd_number] = ((UINT64)InternalAllocatePhysicalPage()) | 7;
		memset(PG_ALIGN(pdp[pd_number]), 0, 4096);
	}
	UINT64** pd = PG_ALIGN(pdp[pd_number]);
	if (pd[pt_number] == 0) {
		pd[pt_number] = ((UINT64)InternalAllocatePhysicalPage()) | 7;
		memset(PG_ALIGN(pd[pt_number]), 0, 4096);
	}

	UINT64* pt = PG_ALIGN(pd[pt_number]);
	pt[page_number] = PG_ALIGN(physical) | flags;
}

void PagingInit() {

	UINT64 CR0 = GetCR0();
	CR0 &= (~65536);
	SetCR0(CR0);

	PML4 = InternalAllocatePhysicalPage();

	memset(PML4, 0, 4096);
	for (int a = 0; a < 512; a++) {
		if (a == 0) {
			PML4e[a] = InternalAllocatePhysicalPage();
			memset(PML4e[a], 0, 4096);
			((UINT64*)PML4e)[a] |= 3;
		}
		else {
			PML4e[a] = 0;
		}
	}
	

	for (int c = 0; c < 20; c++) {
		UINT64** pageDirectory = ((UINT64***)PG_ALIGN(PML4e[0]))[c] = InternalAllocatePhysicalPage();

		for (int a = 0; a < 512; a++) {
			pageDirectory[a] = InternalAllocatePhysicalPage();
			UINT64* pageTable = pageDirectory[a];
			for (int b = 0; b < 512; b++) {
				pageTable[b] = (b + 512 * a + 512 * 512 * c) * 4096;
				pageTable[b] |= 3;
			}
			((UINT64*)pageDirectory)[a] |= 3;
		}
		((UINT64*)PG_ALIGN(PML4e[0]))[c] |= 3;
	}

	for (int a = 0; a < 512; a++) {
		PML4[a] = ((UINT64)(PML4e[a]));

	}
	PML4[511] = PML4;
	((UINT64**)PG_ALIGN(PML4e[0]))[2] = ((UINT64*)PG_ALIGN(GetCR3()[0]))[2]; //interesting, it seems that I can manipulate the location of the framebuffer,
																			 //since its tied up to the physical address.

	SetCR3(PML4);	
}
