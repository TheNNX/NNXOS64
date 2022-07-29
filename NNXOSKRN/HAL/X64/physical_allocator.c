#include <HAL/physical_allocator.h>
#include <hal/spinlock.h>

UINT8* GlobalPhysicalMemoryMap = 0;
UINT64 GlobalPhysicalMemoryMapSize = 0;
UINT64 MemorySize = 0;
KSPIN_LOCK PhysicalAllocatorLock;

ULONG_PTR InternalAllocatePhysicalPageEx(UINT8 type, ULONG_PTR seekFromAddress, ULONG_PTR seekToAddress)
{
    KIRQL irql;

    KeAcquireSpinLock(&PhysicalAllocatorLock, &irql);

    for (UINT8* checkedAddress = GlobalPhysicalMemoryMap + seekFromAddress / 4096U;
		((ULONG_PTR)(checkedAddress - GlobalPhysicalMemoryMap) < GlobalPhysicalMemoryMapSize) && 
		((ULONG_PTR)(checkedAddress - GlobalPhysicalMemoryMap) < (seekToAddress / 4096U));
         checkedAddress++)
	{
        if (*checkedAddress == MEM_TYPE_FREE)
        {
            ULONG_PTR physical;
            *checkedAddress = type;

            physical = (checkedAddress - GlobalPhysicalMemoryMap) * 4096;

            KeReleaseSpinLock(&PhysicalAllocatorLock, irql);
            return physical;
        }
    }

    KeReleaseSpinLock(&PhysicalAllocatorLock, irql);
    return -1;
}

BYTE InternalMarkPhysPage(UINT8 type, ULONG_PTR PhysPage)
{
    KIRQL irql;
    BYTE oldType;
    UINT64 entrynumber = ((UINT64)PhysPage / 4096);

    KeAcquireSpinLock(&PhysicalAllocatorLock, &irql);

    oldType = GlobalPhysicalMemoryMap[entrynumber];
    GlobalPhysicalMemoryMap[entrynumber] = type;

    KeReleaseSpinLock(&PhysicalAllocatorLock, irql);
    return oldType;
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
    KIRQL irql;
    UINT64 entrynumber = ((UINT64) address / 4096);

    KeAcquireSpinLock(&PhysicalAllocatorLock, &irql);

    if (GlobalPhysicalMemoryMapSize <= entrynumber)
    {
        KeReleaseSpinLock(&PhysicalAllocatorLock, irql);
        return -1;
    }
    if (GlobalPhysicalMemoryMap[entrynumber] != MEM_TYPE_USED)
    {
        KeReleaseSpinLock(&PhysicalAllocatorLock, irql);
        return -2;
    }

    GlobalPhysicalMemoryMap[entrynumber] = MEM_TYPE_FREE;

    KeReleaseSpinLock(&PhysicalAllocatorLock, irql);
    return 0;
}