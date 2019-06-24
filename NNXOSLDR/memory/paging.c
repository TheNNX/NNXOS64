#include "paging.h"
#include "video/SimpleTextIO.h"
#include "physical_allocator.h"
#include "MemoryOperations.h"

UINT64**** PML4;
UINT64**** PML4_IdentifyMap;

void PagingMapPage(void* virtual, void* physical, UINT16 flags) {
	UINT64**** pml4 = GetCR3();
	SetCR3(PML4_IdentifyMap);
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
	SetCR3(pml4);
}

void PagingInit() {

	UINT64 CR0 = GetCR0();
	CR0 &= (~65536);
	SetCR0(CR0);
	PML4 = InternalAllocatePhysicalPage();
	PML4_IdentifyMap = InternalAllocatePhysicalPage();

	memset(PML4, 0, 4096);
	memset(PML4_IdentifyMap, 0, 4096);

	for (int a = 0; a < 512; a++) {
		PML4[a] = InternalAllocatePhysicalPage();
		memset(PML4[a], 0, 4096);
		((UINT64*)PML4)[a] |= 3;

		PML4_IdentifyMap[a] = InternalAllocatePhysicalPage();
		memset(PML4_IdentifyMap[a], 0, 4096);
		((UINT64*)PML4_IdentifyMap)[a] |= 3;
	}

	int memSizeInPDPs = ((MemorySize-1) / 4096 / 512 / 512 / 512 + 1) %512;
	for (UINT64 d = 0; d < memSizeInPDPs; d++) {
		int memSizeInPageDirs = (d + 1 == memSizeInPDPs) ?  ((MemorySize-1) / 4096 / 512 / 512 + 1) % 512 : 512;

		for (UINT64 c = 0; c < memSizeInPageDirs; c++) {
			UINT64** pageDirectory = ((UINT64***)PG_ALIGN(PML4[d]))[c] = InternalAllocatePhysicalPage();
			UINT64** pageDirectoryIM = ((UINT64***)PG_ALIGN(PML4_IdentifyMap[d]))[c] = InternalAllocatePhysicalPage();
			for (UINT64 a = 0; a < 512; a++) {
				pageDirectory[a] = InternalAllocatePhysicalPage();
				pageDirectoryIM[a] = InternalAllocatePhysicalPage();
				UINT64* pageTable = pageDirectory[a];
				UINT64* pageTableIM = pageDirectoryIM[a];
				for (UINT64 b = 0; b < 512; b++) {
					pageTableIM[b] = pageTable[b] = (b + 512 * a + 512 * 512 * c + d * 512 * 512 * 512) * 4096;
					pageTable[b] |= 3; //to be optimized, this paging structures set don't need to cover all of the memory
					pageTableIM[b] |= 3;
				}
				((UINT64*)pageDirectory)[a] |= 3;
				((UINT64*)pageDirectoryIM)[a] |= 3;
			}
			((UINT64*)PG_ALIGN(PML4[d]))[c] |= 3;
			((UINT64*)PG_ALIGN(PML4_IdentifyMap[d]))[c] |= 3;
			if (!(((d + 1)*512 + c + 1) % ((memSizeInPDPs / 3 + 1) * 512 + (memSizeInPageDirs / 3 + 1)))) {
				PrintT(".");
			}
		}

	}
	SetCR3(PML4);

	for (int i = 0; i < FrameBufferSize()/4096+1; i++) {
		PagingMapPage(FRAMEBUFFER_DESIRED_LOCATION+i*4096, ((UINT64)framebuffer)+i*4096, 0x3);
	}
	TextIOInitialize(FRAMEBUFFER_DESIRED_LOCATION, FRAMEBUFFER_DESIRED_LOCATION + ((FrameBufferSize()/4096)+1)*4096, 800, 600);
}
