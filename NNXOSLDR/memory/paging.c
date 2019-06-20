#include "paging.h"
#include "video/SimpleTextIO.h"

__declspec(align(4096)) UINT64*** PML4[512] = {0};
__declspec(align(4096)) UINT64** PML4e[512][512];
__declspec(align(4096)) UINT64* temp[512] = { 0 };
__declspec(align(4096)) UINT64 temp2[512] = { 0 };

void PagingMapPage(void* virtual, void* physical, UINT16 flags) {
	UINT64**** pml4 = GetCR3();
	UINT64 pdp_number = (((UINT64)virtual) & PML4_ADDRESS_MASK) >> 39;
	UINT64 pd_number = (((UINT64)virtual) & PDP_ADDRESS_MASK) >> 30;
	UINT64 pt_number = (((UINT64)virtual) & PD_ADDRESS_MASK) >> 21;
	UINT64 page_number = (((UINT64)virtual) & PT_ADDRESS_MASK) >> 12;
	UINT64*** pdp = PG_ALIGN(PML4[pdp_number]);
	UINT64** pd = PG_ALIGN(pdp[pd_number]);
	UINT64* pt = PG_ALIGN(pd[pt_number]);
	pt[page_number] = PG_ALIGN(physical) | flags;
}

void PagingInit() {
	UINT64*** sourceCR3 = GetCR3();
	UINT64** sourceFisrtEntryOfPML4 = sourceCR3[0];
	for (int a = 0; a < 512; a++) {
		if (!sourceCR3[a]) continue;
		for (int b = 0; b < 512; b++) {
			PML4e[a][b] = ((UINT64***)(((UINT64)sourceCR3[a])&(~(0xfff))))[b];
		}
	}
	
	for (int a = 0; a < 512; a++) {
		if (!sourceCR3[a]) continue;
		PML4[a] = ((UINT64)&(PML4e[a])) | 0x7;
	}
	PML4[511] = 0;
	
	//All PTs seems to be empty, I have no idea how is this memory identify mapped... I'll create my own tables then, I guess...
	for (int a = 0; a < 512; a++) {
		PrintT("%x     ",((UINT64*)PG_ALIGN(((UINT64*)PG_ALIGN(PML4e[0][0]))[511]))[a]);
	}
	((UINT64*)PG_ALIGN(PML4e[0][0]))[10] = ((UINT64)temp) | 7;
	temp[1] = ((UINT64)temp2) | 7;

	SetCR3(PML4);
}

void PagingTest() {
	
	UINT64* vrt = 0x1403000;
	UINT64* phy = 0x5000;
	PagingMapPage(vrt, phy, PAGE_PRESENT | PAGE_WRITE | PAGE_SYSTEM);
	*vrt = 0x4254;

	PrintT("Test: v%x ? p%x\n",*vrt,*phy);
}