#include <HAL/physical_allocator.h>
#include <HAL/spinlock.h>
#include <rtl/rtl.h>
#include <HAL/paging.h>

static MMPFN_LIST FreeList;
static MMPFN_LIST WorkingsetList;
static KSPIN_LOCK PfnEntriesLock;
PMMPFN_ENTRY PfnEntries;
SIZE_T NumberOfPfnEntries;

/**
 * @brief This function initializes the physical memory allocator. 
 * 
 * @param pfnEntries - a pointer to an array of preallocated
 * MMPFN_ENTRY strucutres. The flags member has to be non-zero, should
 * the page frame start as an allocated one.
 * @param numberOfPfnEntries - number of entries in the PfnEntires array
 */
VOID MmReinitPhysAllocator(
    PMMPFN_ENTRY pfnEntries,
    SIZE_T numberOfPfnEntries
)
{
    PFN_NUMBER pfnIndex;

    /* initialze the list heads*/
    InitializeListHead(&WorkingsetList.PfnListHead);
    InitializeListHead(&FreeList.PfnListHead);

    FreeList.NumberOfPfns = numberOfPfnEntries;
    WorkingsetList.NumberOfPfns = 0;

    /* intialize the PFN entries */
    for (pfnIndex = 0; pfnIndex < numberOfPfnEntries; pfnIndex++)
    {   
        PMMPFN_LIST selectedList;

        if (pfnEntries[pfnIndex].Flags != 0)
        {
            selectedList = &WorkingsetList;
            pfnEntries[pfnIndex].Flags = MMPFN_FLAG_PERMAMENT | MMPFN_FLAG_NO_PAGEOUT;
        }
        else
        {
            selectedList = &FreeList;
            pfnEntries[pfnIndex].Flags = 0;
        }

        pfnEntries[pfnIndex].InList = selectedList;

        InsertTailList(
            &selectedList->PfnListHead,
            &pfnEntries[pfnIndex].ListEntry
        );
    }

    PfnEntries = pfnEntries;
    NumberOfPfnEntries = numberOfPfnEntries;
    KeInitializeSpinLock(&PfnEntriesLock);
}

/**
 * @brief This function allocates one page frame number from the free pages list.
 */
NTSTATUS MmAllocatePfn(PFN_NUMBER* pPfnNumber)
{
    KIRQL irql;
    PMMPFN_ENTRY listEntry;
    NTSTATUS result;

    KeAcquireSpinLock(&PfnEntriesLock, &irql);

    if (IsListEmpty(&FreeList.PfnListHead))
    {
        result = STATUS_NO_MEMORY;
    }
    else
    {
        listEntry = (PMMPFN_ENTRY)RemoveHeadList(&FreeList.PfnListHead);
        listEntry->InList->NumberOfPfns--;
        listEntry->InList = &WorkingsetList;
        listEntry->InList->NumberOfPfns++;
        listEntry->Flags = 4;
        InsertTailList(&WorkingsetList.PfnListHead, &listEntry->ListEntry);
        *pPfnNumber = (PFN_NUMBER)(listEntry - PfnEntries);

        result = STATUS_SUCCESS;
    }
    KeReleaseSpinLock(&PfnEntriesLock, irql);
    
    return result;
}

NTSTATUS MmFreePfn(PFN_NUMBER pfnNumber)
{
    KIRQL irql;
    PMMPFN_ENTRY entry;
    NTSTATUS result;

    entry = PfnEntries + pfnNumber;

    KeAcquireSpinLock(&PfnEntriesLock, &irql);

    if (entry->InList == &FreeList ||
        (entry->Flags & MMPFN_FLAG_PERMAMENT) != 0)
    {
        result = STATUS_INVALID_PARAMETER;
    }
    else
    {
        RemoveEntryList(&entry->ListEntry);
        entry->InList->NumberOfPfns--;
        entry->InList = &FreeList;
        entry->InList->NumberOfPfns++;
        InsertTailList(&FreeList.PfnListHead, &entry->ListEntry);
        result = STATUS_SUCCESS;
    }

    KeReleaseSpinLock(&PfnEntriesLock, irql);
    return result;
}

NTSTATUS MmAllocatePhysicalAddress(ULONG_PTR* pPhysAddress)
{
    PFN_NUMBER pfn;
    NTSTATUS status;

    status = MmAllocatePfn(&pfn);
    if (!NT_SUCCESS(status))
        return status;

    *pPhysAddress = PA_FROM_PFN(pfn);
    return STATUS_SUCCESS;
}

NTSTATUS MmFreePhysicalAddress(ULONG_PTR physAddress)
{
    return MmFreePfn(PFN_FROM_PA(physAddress));
}

NTSTATUS MmMarkPfnAsUsed(PFN_NUMBER pfnNumber)
{
    PMMPFN_ENTRY entry;
    NTSTATUS result;
    KIRQL irql;

    if (pfnNumber >= NumberOfPfnEntries)
        return STATUS_INVALID_PARAMETER;

    KeAcquireSpinLock(&PfnEntriesLock, &irql);
    entry = &PfnEntries[pfnNumber];
    
    if (entry->InList != &FreeList)
    {
        result = STATUS_INVALID_PARAMETER;
    }
    else
    {
        entry->InList->NumberOfPfns--;
        entry->InList = &WorkingsetList;
        entry->InList->NumberOfPfns++;
        InsertTailList(&WorkingsetList.PfnListHead, &entry->ListEntry);
        result = STATUS_SUCCESS;
    }

    KeReleaseSpinLock(&PfnEntriesLock, irql);
    return result;
}

VOID MiFlagPfnsForRemap()
{
    PFN_NUMBER i;

    for (i = 0; i < NumberOfPfnEntries; i++)
    {
        if (PfnEntries[i].InList == &FreeList)
        {
            PfnEntries[i].Flags = 0;
        }
        else
        {
            PfnEntries[i].Flags = 1;
        }
    }
}

VOID DrawMap()
{
	UINT x = 0;
	UINT y = 0;
	UINT a;

	TextIoGetCursorPosition(&x, &y);

	x = 0;
	y += 10;

	for (a = 0; a < NumberOfPfnEntries; a++)
	{
		if (PfnEntries[a].InList == &FreeList)
			gFramebuffer[x + y * gPixelsPerScanline] = 0xFF007F00;
        else
			gFramebuffer[x + y * gPixelsPerScanline] = 0xFF7F0000;
		x++;
		if (x > gWidth)
		{
			y++;
			x = 0;
		}
	}
}