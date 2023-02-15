#include <HAL/paging.h>
#include <HAL/spinlock.h>
#include <device/fs/vfs.h>
#include <pool.h>
#include <rtl/rtl.h>
#include <bugcheck.h>
#include <HAL/X64/registers.h>
#include <HAL/X64/IDT.h>
#include <HAL/physical_allocator.h>
#include <HAL/cpu.h>

SIZE_T PageFileSize;
KSPIN_LOCK PageFileLock;
VFS_FILE* PageFile;
PBYTE PageFileMap;

static
BOOL
PagingSetPageFileMapBit(
    ULONG_PTR pageFilePageIndex,
    BOOL value
);

static
ULONG_PTR
PagingSelectPageFilePageIndex();

static
BOOL
PagingGetPageFileMapBit(
    ULONG_PTR pageFilePageIndex
);

NTSTATUS
PagingInitializePageFile(
    SIZE_T pageFileSize,
    const char* filePath,
    VIRTUAL_FILE_SYSTEM* filesystem
)
{
    SIZE_T pageFileNumberOfPages;

    PageFileSize = pageFileSize;
    KeInitializeSpinLock(&PageFileLock);

    PageFile = filesystem->Functions.OpenOrCreateFile(filesystem, filePath);
    if (PageFile == NULL)
    {
        PrintT("Creating page file failed\n");
        while (1);
    }
    filesystem->Functions.ResizeFile(PageFile, PageFileSize);

    pageFileNumberOfPages = PageFileSize / PAGE_SIZE;

    PageFileMap = ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(*PageFileMap) * pageFileNumberOfPages / 8,
        'MMGR'
    );

    RtlZeroMemory(PageFileMap, sizeof(*PageFileMap) * pageFileNumberOfPages / 8);
    return STATUS_SUCCESS;
}

static
ULONG_PTR
PagingSelectPageFilePageIndex()
{
    ULONG_PTR currentCheckedIndex;


    if (PageFileLock == 0)
        KeBugCheckEx(SPIN_LOCK_NOT_OWNED, __LINE__, 0, 0, 0);

    for (currentCheckedIndex = 1; currentCheckedIndex < PageFileSize / PAGE_SIZE; currentCheckedIndex++)
    {
        if (PagingGetPageFileMapBit(currentCheckedIndex) == 0)
        {
            if (PagingSetPageFileMapBit(currentCheckedIndex, TRUE) == FALSE)
                return -1;
            return currentCheckedIndex;
        }
    }

    return -1;
}

static
BOOL
PagingSetPageFileMapBit(
    ULONG_PTR pageFilePageIndex,
    BOOL value
)
{
    SIZE_T containingCellOffset;
    SIZE_T bitNumber;

    if (PageFileLock == 0)
        KeBugCheckEx(SPIN_LOCK_NOT_OWNED, __LINE__, 0, 0, 0);

    containingCellOffset = pageFilePageIndex / (sizeof(*PageFileMap) * 8);
    if (containingCellOffset > PageFileSize / PAGE_SIZE / 8)
    {
        return FALSE;
    }
    bitNumber = pageFilePageIndex % (sizeof(*PageFileMap) * 8);

    if (value == FALSE)
        PageFileMap[containingCellOffset] &= ~(1 << bitNumber);
    else
        PageFileMap[containingCellOffset] |= (1 << bitNumber);

    return TRUE;
}

static
BOOL
PagingGetPageFileMapBit(
    ULONG_PTR pageFilePageIndex
)
{
    SIZE_T containingCellOffset;
    SIZE_T bitNumber;

    if (PageFileLock == 0)
        KeBugCheckEx(SPIN_LOCK_NOT_OWNED, __LINE__, 0, 0, 0);

    containingCellOffset = pageFilePageIndex / (sizeof(*PageFileMap) * 8);
    if (containingCellOffset > PageFileSize / PAGE_SIZE / 8)
    {
        return FALSE;
    }
    bitNumber = pageFilePageIndex % (sizeof(*PageFileMap) * 8);

    return (PageFileMap[containingCellOffset] & (1 << bitNumber));
}

static
NTSTATUS
PagingSavePageToPageFile(
    ULONG_PTR virtualAddress,
    ULONG_PTR pageFilePageIndex
)
{
    VFS* filesystem;

    if (PageFileLock == 0)
        KeBugCheckEx(SPIN_LOCK_NOT_OWNED, __LINE__, 0, 0, 0);

    filesystem = PageFile->Filesystem;
    PageFile->FilePointer = PAGE_SIZE * pageFilePageIndex;
    filesystem->Functions.WriteFile(PageFile, PAGE_SIZE, (PVOID)virtualAddress);

    return STATUS_SUCCESS;
}

static
NTSTATUS
PagingLoadPageFromPageFile(
    ULONG_PTR virtualAddress,
    ULONG_PTR pageFilePageIndex
)
{
    VFS* filesystem;

    if (PageFileLock == 0)
        KeBugCheckEx(SPIN_LOCK_NOT_OWNED, __LINE__, 0, 0, 0);

    filesystem = PageFile->Filesystem;
    PageFile->FilePointer = PAGE_SIZE * pageFilePageIndex;
    filesystem->Functions.ReadFile(PageFile, PAGE_SIZE, (PVOID)virtualAddress);

    return STATUS_SUCCESS;
}

NTSTATUS
PagingPageOutPage(
    ULONG_PTR virtualAddress
)
{
    ULONG_PTR pageFilePageIndex;
    ULONG_PTR oldMapping;
    NTSTATUS status;
    USHORT oldFlags;
    KIRQL irql;

    KeAcquireSpinLock(&PageFileLock, &irql);

    /* allocate an index in the pagefile */
    pageFilePageIndex = PagingSelectPageFilePageIndex();
    /* save the page to this index */
    status = PagingSavePageToPageFile(virtualAddress, pageFilePageIndex);

    if (status != STATUS_SUCCESS)
    {
        /* if saving failed, free the allocated index */
        PagingSetPageFileMapBit(pageFilePageIndex, FALSE);
        KeReleaseSpinLock(&PageFileLock, irql);
        /* and return the error code */
        return status;
    }

    /* get the old page mapping */
    oldMapping = PagingGetCurrentMapping(virtualAddress);
    /* get the old flags (but clear the present flag) */
    oldFlags = (oldMapping & 0xFFF) & (~PAGE_PRESENT);
    oldMapping &= (~0xFFF);
    status = MmFreePhysicalAddress(oldMapping);
    if (!NT_SUCCESS(status))
    {
        KeReleaseSpinLock(&PageFileLock, irql);
        return status;
    }

    /* Change the mapping to pageFilePageIndex * PAGE_SIZE and preserve the flags, but clear the PRESENT flag
     * This way, the page file index is easily derived from the paging structures, 
     * and the flags can be easily restoed */
    PagingMapPage(virtualAddress, pageFilePageIndex * PAGE_SIZE, oldFlags);

    KeReleaseSpinLock(&PageFileLock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS
PagingPageInPage(
    ULONG_PTR virtualAddress
)
{
    ULONG_PTR pageFilePageIndex;
    ULONG_PTR tempMapping;
    USHORT originalFlags;
    PFN_NUMBER pfnNumber;
    ULONG_PTR physAddr;
    NTSTATUS status;
    KIRQL irql;
 
    KeAcquireSpinLock(&PageFileLock, &irql);

    /* Get the temp page-file page mapping */
    tempMapping = PagingGetCurrentMapping(virtualAddress & PAGE_ADDRESS_MASK);
    /* Get the old flags (but clear the present flag) */
    originalFlags = (tempMapping & 0xFFF) & (~PAGE_PRESENT);
    tempMapping &= PAGE_ADDRESS_MASK;

    /* Get the page file page index */
    pageFilePageIndex = tempMapping / PAGE_SIZE;

    /* No such page file page index allocated, or the index is invalid */
    if (pageFilePageIndex == 0 ||
        PagingGetPageFileMapBit(pageFilePageIndex) == FALSE)
    {
        KeReleaseSpinLock(&PageFileLock, irql);
        return STATUS_INVALID_PARAMETER;
    }

    status = MmAllocatePfn(&pfnNumber);
    if (!NT_SUCCESS(status))
    {
        KeReleaseSpinLock(&PageFileLock, irql);
        return status;
    }
    physAddr = PA_FROM_PFN(pfnNumber);

    /* Map the page */
    PagingMapPage(virtualAddress, physAddr, originalFlags | PAGE_PRESENT);

    /* Read the page from the page file */
    status = PagingLoadPageFromPageFile(virtualAddress, pageFilePageIndex);

    if (!NT_SUCCESS(status))
    {
        KeReleaseSpinLock(&PageFileLock, irql);
        return status;
    }

    /* free the page file index */
    PagingSetPageFileMapBit(pageFilePageIndex, FALSE);
    KeReleaseSpinLock(&PageFileLock, irql);
    return STATUS_SUCCESS;
}

VOID
PageFaultHandler(
    ULONG_PTR n,
    ULONG_PTR errcode,
    ULONG_PTR errcode2,
    ULONG_PTR rip
)
{
    NTSTATUS status;

    PrintT("Servicing page fault %X %X %X\n", rip, errcode, KeGetCurrentProcessorId());
    if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
    {
        KeBugCheckEx(
            PAGE_FAULT_WITH_INTERRUPTS_OFF,
            (ULONG_PTR)errcode,
            (ULONG_PTR)rip,
            (ULONG_PTR)n,
            (ULONG_PTR)KeGetCurrentIrql()
        );
    }

    if ((errcode & PAGE_PRESENT) == 0)
    {
        status = PagingPageInPage(GetCR2());
        if (status)
        {
            KeBugCheckEx(PAGE_FAULT_IN_NONPAGED_AREA, GetCR2(), 1, status, rip);
        }
    }
    else
    {
        KeBugCheckEx(PAGE_FAULT_IN_NONPAGED_AREA, GetCR2(), 2, 0, rip);
    }
}