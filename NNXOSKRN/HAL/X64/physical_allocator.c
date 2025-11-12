#include <physical_allocator.h>
#include <spinlock.h>
#include <rtl.h>
#include <paging.h>
#include <SimpleTextIO.h>

PMMPFN_ENTRY PfnEntries;
SIZE_T       NumberOfPfnEntries;
static MMPFN_LIST   FreeList;
static MMPFN_LIST   WorkingsetList;
static KSPIN_LOCK   PfnEntriesLock;

#define MM_PFN_NEXT_ENTRY(entry) CONTAINING_RECORD((entry)->ListEntry.Next, \
                                                   MMPFN_ENTRY, \
                                                   ListEntry)

/**
 * @brief This function initializes the physical memory allocator. 
 * 
 * @param pfnEntries A pointer to an array of preallocated MMPFN_ENTRY 
 *                   strucutres. The flags member has to be non-zero, should 
 *                   the page frame start as an allocated one.
 * @param numberOfPfnEntries The number of entries in the PfnEntires array
 */
VOID 
NTAPI
MmReinitPhysAllocator(PMMPFN_ENTRY pfnEntries,
                      SIZE_T numberOfPfnEntries)
{
    PFN_NUMBER pfnIndex;

    /* Initialze the list heads. */
    InitializeListHead(&WorkingsetList.PfnListHead);
    InitializeListHead(&FreeList.PfnListHead);

    FreeList.NumberOfPfns = numberOfPfnEntries;
    WorkingsetList.NumberOfPfns = 0;

    /* Intialize the PFN entries. */
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

        InsertTailList(&selectedList->PfnListHead,
                       &pfnEntries[pfnIndex].ListEntry);
    }

    PfnEntries = pfnEntries;
    NumberOfPfnEntries = numberOfPfnEntries;
    KeInitializeSpinLock(&PfnEntriesLock);
}

/**
 * @brief This function allocates one page frame number from the free pages list.
 */
NTSTATUS 
NTAPI
MmAllocatePfn(PFN_NUMBER* pPfnNumber)
{
    PMMPFN_ENTRY listEntry;
    NTSTATUS result;

    KeAcquireSpinLockAtDpcLevel(&PfnEntriesLock);

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
        listEntry->Flags = 0;
        InsertTailList(&WorkingsetList.PfnListHead, &listEntry->ListEntry);
        *pPfnNumber = (PFN_NUMBER)(listEntry - PfnEntries);

        result = STATUS_SUCCESS;
    }
    KeReleaseSpinLockFromDpcLevel(&PfnEntriesLock);
    
    return result;
}

static
VOID
MiLockedAllocRange(PMMPFN_ENTRY Start, PMMPFN_ENTRY End)
{
    PMMPFN_ENTRY Current = Start, Next = NULL;

    while (Current != End)
    {
        Next = MM_PFN_NEXT_ENTRY(Current);

        RemoveEntryList(&Current->ListEntry);
        Current->InList->NumberOfPfns--;
        Current->InList = &WorkingsetList;
        Current->InList->NumberOfPfns++;
        Current->Flags = 4;
        InsertTailList(&WorkingsetList.PfnListHead, &Current->ListEntry);

        PrintT("Allocating %X(%X) for contiguous block\n", PA_FROM_PFN(Current - PfnEntries), Current);

        Current = Next;
    }
}

/**
 * @brief This function finds, and allocates a contiguous region of physical 
 *        memory.
 * 
 * @param nRegionPages Number of pages to allocate
 * @param[out] pPfnNumber If the function is successful, this pointer receives 
 *                        the PFN of the first page in the allocated region.
 * @param Flags A bitwise OR of zero or more following values:
 *               - MM_ALLOC_PFN_REGION_4GB - The allocated region must reside
 *                                           in the lower 4GB of physical memory.
 */
NTSTATUS
NTAPI
MmAllocatePfnRegion(SIZE_T nRegionPages, 
                    PFN_NUMBER* pPfnNumber, 
                    UINT Flags)
{
    PLIST_ENTRY ListEntry;
    SIZE_T ContiguousPrefix;
    PFN_NUMBER LastPfn, CurrentPfn;
    PMMPFN_ENTRY MmPfnEntry;
    PMMPFN_ENTRY PrefixStart;

    KeAcquireSpinLockAtDpcLevel(&PfnEntriesLock);

    if (IsListEmpty(&FreeList.PfnListHead))
    {
        KeReleaseSpinLockFromDpcLevel(&PfnEntriesLock);
        return STATUS_NO_MEMORY;
    }
    
    ListEntry = FreeList.PfnListHead.First;
    ContiguousPrefix = 0;
    LastPfn = 0;
    PrefixStart = NULL;

    while (ListEntry != &FreeList.PfnListHead)
    {
        MmPfnEntry = CONTAINING_RECORD(ListEntry, MMPFN_ENTRY, ListEntry);
        CurrentPfn = (PFN_NUMBER)(MmPfnEntry - PfnEntries);

        if (PrefixStart != NULL && 
            LastPfn + 1 == CurrentPfn && 
            (!(Flags & MM_ALLOC_PFN_REGION_4GB) || 
                PA_FROM_PFN(CurrentPfn) <= 0xFFFFFFFF))
        {
            ContiguousPrefix++;
        }
        else
        {
            ContiguousPrefix = 0;
            PrefixStart = MmPfnEntry;
        }

        LastPfn = CurrentPfn;
        ListEntry = ListEntry->Next;

        if (ContiguousPrefix + 1 >= nRegionPages)
        {
            PrintT("Found range for %i pages:\n", nRegionPages);
            MiLockedAllocRange(PrefixStart, MM_PFN_NEXT_ENTRY(MmPfnEntry));
            
            *pPfnNumber = (PFN_NUMBER)(PrefixStart - PfnEntries);
            KeReleaseSpinLockFromDpcLevel(&PfnEntriesLock);
            return STATUS_SUCCESS;
        }
    }

    KeReleaseSpinLockFromDpcLevel(&PfnEntriesLock);
    return STATUS_NO_MEMORY;
}

NTSTATUS 
NTAPI
MmFreePfn(PFN_NUMBER pfnNumber)
{
    PMMPFN_ENTRY entry;
    NTSTATUS result;

    entry = PfnEntries + pfnNumber;

    KeAcquireSpinLockAtDpcLevel(&PfnEntriesLock);

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

    KeReleaseSpinLockFromDpcLevel(&PfnEntriesLock);
    return result;
}

NTSTATUS 
NTAPI
MmAllocatePhysicalAddress(ULONG_PTR* pPhysAddress)
{
    PFN_NUMBER pfn;
    NTSTATUS status;

    status = MmAllocatePfn(&pfn);
    if (!NT_SUCCESS(status))
        return status;

    *pPhysAddress = PA_FROM_PFN(pfn);
    return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI
MmFreePhysicalAddress(ULONG_PTR physAddress)
{
    return MmFreePfn(PFN_FROM_PA(physAddress));
}

NTSTATUS
NTAPI
MmMarkPfnAsUsed(PFN_NUMBER pfnNumber)
{
    PMMPFN_ENTRY entry;
    NTSTATUS result;

    if (pfnNumber >= NumberOfPfnEntries)
        return STATUS_INVALID_PARAMETER;

    KeAcquireSpinLockAtDpcLevel(&PfnEntriesLock);
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

    KeReleaseSpinLockFromDpcLevel(&PfnEntriesLock);
    return result;
}

VOID 
NTAPI
MiFlagPfnsForRemap()
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
            PfnEntries[i].Flags = MMPFN_FLAG_NO_PAGEOUT;
        }
    }
}

VOID 
DrawMap()
{
    UINT x = 0;
    UINT y = 0;
    UINT a;

    TextIoGetCursorPosition(&x, &y);

    x = 0;
    y += 10;
    PrintT("Physical memory layout:\n");
    for (a = 0; a < NumberOfPfnEntries; a++)
    {
        if (y > gHeight || a > 10000)
        {
            PrintT("Too much memory to finish drawing the map.\n");
            break;
        }

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