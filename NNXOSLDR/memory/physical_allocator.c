#include "physical_allocator.h"

UINT8* GlobalPhysicalMemoryMap = 0;
UINT64 GlobalPhysicalMemoryMapSize = 0;
UINT64 MemorySize = 0;

void* InternalAllocatePhysicalPage() {
	for (UINT8* checkedAddress = GlobalPhysicalMemoryMap; checkedAddress < GlobalPhysicalMemoryMapSize + GlobalPhysicalMemoryMap; checkedAddress++) {
		if (*checkedAddress == 1 || *checkedAddress == 2) {
			*checkedAddress = 0;
			return (checkedAddress - GlobalPhysicalMemoryMap)*4096;
		}
	}
}

UINT8 InternalFreePhysicalPage(void* address) {
	int entrynumber = ((UINT64)address / 4096);
	if (GlobalPhysicalMemoryMapSize <= entrynumber)
		return -1;
	if (GlobalPhysicalMemoryMap[entrynumber] == 1)
		return -2;
	GlobalPhysicalMemoryMap[entrynumber] = 1;
	return 0;
}