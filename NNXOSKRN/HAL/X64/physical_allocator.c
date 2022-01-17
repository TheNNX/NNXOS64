#include <HAL/physical_allocator.h>

UINT8* GlobalPhysicalMemoryMap = 0;
UINT64 GlobalPhysicalMemoryMapSize = 0;
UINT64 MemorySize = 0;

ULONG_PTR InternalAllocatePhysicalPageEx(UINT8 type, UINT64 seekFromAddress, UINT64 seekToAddress)
{
    for (UINT8* checkedAddress = GlobalPhysicalMemoryMap + seekFromAddress / 4096U;
		((ULONG_PTR)(checkedAddress - GlobalPhysicalMemoryMap) < GlobalPhysicalMemoryMapSize) && 
		((ULONG_PTR)(checkedAddress - GlobalPhysicalMemoryMap) < (seekToAddress / 4096U));
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

ULONG_PTR InternalAllocatePhysicalPageWithType(UINT8 type)
{
    return InternalAllocatePhysicalPageEx(type, 4096, 0x7FFFFFFFFFFFFFFF);
}

ULONG_PTR InternalAllocatePhysicalPage()
{
    return InternalAllocatePhysicalPageWithType(MEM_TYPE_USED);
}

UINT8 InternalFreePhysicalPage(ULONG_PTR address)
{
    UINT64 entrynumber = ((UINT64) address / 4096);
    if (GlobalPhysicalMemoryMapSize <= entrynumber)
        return -1;
    if (GlobalPhysicalMemoryMap[entrynumber] != MEM_TYPE_USED)
        return -2;
    GlobalPhysicalMemoryMap[entrynumber] = MEM_TYPE_FREE;
    return 0;
}