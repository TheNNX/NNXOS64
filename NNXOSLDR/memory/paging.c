#include "paging.h"
#include "video/SimpleTextIO.h"

__declspec(align(4096)) UINT64*** PML4[512];
__declspec(align(4096)) UINT64** PML4e[512];

void PagingInit() {
	UINT64*** sourceCR3 = GetCR3();
	UINT64** sourceFisrtEntryOfPML4 = sourceCR3[0];
	for (int a = 0; a < 512; a++) {
		PML4[a] = sourceCR3[a];
		PML4e[a] = ((UINT64***)(((UINT64)PML4[0])&(~(0xfff))))[a];
	}
	PML4[2] = ((UINT64)PML4e)| 0x7;
	//PML4[0][511] = 0;
	//PML4[7] = PML4[0];
	PrintT("\n%x %x\n", PML4[0], PML4[1]);
	SetCR3(PML4);
	*((UINT64*)0x10000000000) = 0x839;
	PrintT("%x\n", *((UINT64*)0));
	//PML4[7][2] = 0;
}

void PagingTest() {
	PrintT("Test");
}