#include "physical_allocator.h"

UINT8* GlobalPhysicalMemoryMap = 0;
UINT64 GlobalPhysicalMemoryMapSize = 0;
UINT64 MemorySize = 0;

void* InternalAllocatePhysicalPageEx(UINT8 type, UINT64 seekFromAddress, UINT64 seekToAddress)
{
	for (UINT8* checkedAddress = GlobalPhysicalMemoryMap + seekFromAddress / 4096; 
		(checkedAddress - GlobalPhysicalMemoryMap < GlobalPhysicalMemoryMapSize) && (checkedAddress - GlobalPhysicalMemoryMap < (seekToAddress / 4096));
		checkedAddress++) 
	{
		if (*checkedAddress == MEM_TYPE_FREE) 
		{
			*checkedAddress = type;
			return (checkedAddress - GlobalPhysicalMemoryMap) * 4096;
		}
	}

	return -1;
}

void* InternalAllocatePhysicalPageWithType(UINT8 type) 
{
	return InternalAllocatePhysicalPageEx(type, 4096, 0x7FFFFFFFFFFFFFFF);
}

void* InternalAllocatePhysicalPage() 
{
	return InternalAllocatePhysicalPageWithType(MEM_TYPE_USED);
}

UINT8 InternalFreePhysicalPage(void* address) 
{
	int entrynumber = ((UINT64)address / 4096);
	if (GlobalPhysicalMemoryMapSize <= entrynumber)
		return -1;
	if (GlobalPhysicalMemoryMap[entrynumber] != MEM_TYPE_USED)
		return -2;
	GlobalPhysicalMemoryMap[entrynumber] = MEM_TYPE_FREE;
	return 0;
}