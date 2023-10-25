#include <paging.h>
#include <SimpleTextIo.h>
#include <physical_allocator.h>
#include <rtl.h>
#include <spinlock.h>
#include <HALX64/include/msr.h>
#include <bugcheck.h>
#include <scheduler.h>
#include <ntdebug.h>

#define PML4EntryForRecursivePaging 510ULL
#define PML4_COVERED_SIZE 0x1000000000000ULL
#define PDP_COVERED_SIZE 0x8000000000ULL
#define PD_COVERED_SIZE 0x40000000ULL
#define PT_COVERED_SIZE 0x200000ULL
#define PAGE_SIZE_SMALL 4096ULL

/**
 * Virtual memory layout:
 *
 * PML4:
 * PDPs 0-509 : pages
 * PDP  510   : paging structures
 * PDP  511   : kernel space
 */

#define RecursivePagingPagesSizePreBoundary ((UINT64*)ToCanonicalAddress(PDP_COVERED_SIZE * PML4EntryForRecursivePaging))

#define RecursivePagingPTsBase              ((UINT64*)ToCanonicalAddress((UINT64)RecursivePagingPagesSizePreBoundary))
#define RecursivePagingPTsSizePreBoundary   PD_COVERED_SIZE * PML4EntryForRecursivePaging

#define RecursivePagingPDsBase              ((UINT64*)ToCanonicalAddress((UINT64)RecursivePagingPTsBase + (UINT64)RecursivePagingPTsSizePreBoundary))
#define RecursivePagingPDsSizePreBoundary   PT_COVERED_SIZE * PML4EntryForRecursivePaging

#define RecursivePagingPDPsBase             ((UINT64*)ToCanonicalAddress((UINT64)RecursivePagingPDsBase + (UINT64)RecursivePagingPDsSizePreBoundary))
#define RecursivePagingPDPsSizePreBoundary  PAGE_SIZE_SMALL * PML4EntryForRecursivePaging

#define RecursivePagingPML4Base             ((UINT64*)ToCanonicalAddress((UINT64)RecursivePagingPDPsBase + (UINT64)RecursivePagingPDPsSizePreBoundary))
#define RecursivePagingPML4Size             PAGE_SIZE_SMALL

KSPIN_LOCK PagingSpinlock;
ULONG_PTR KeKernelPhysicalAddress;
ULONG_PTR KernelPml4Entry = NULL;

VOID 
NTAPI
PagingTLBFlushPage(
    UINT64 page)
{
    /* TODO: send IPI to other processors if neccessary */
    PagingInvalidatePage(ToCanonicalAddress(page));
}

BOOL PagingCheckIsPageIndexFree(UINT64 pageIndex)
{
    if ((RecursivePagingPML4Base[pageIndex / (512ULL * 512ULL * 512ULL)]) == 0)
    {
        return TRUE;
    }
    if ((RecursivePagingPDPsBase[pageIndex / (512 * 512)]) == 0)
    {
        return TRUE;
    }
    if ((RecursivePagingPDsBase[pageIndex / 512]) == 0)
    {
        return TRUE;
    }
    if ((RecursivePagingPTsBase[pageIndex]) == 0)
    {
        return TRUE;
    }

    return FALSE;
}

NTSTATUS 
NTAPI
MmCreateAddressSpace(
    PADDRESS_SPACE pOutAddressSpace)
{
    ULONG_PTR PhysicalPML4;
    NTSTATUS Status;

    KeInitializeSpinLock(&pOutAddressSpace->Lock);

    Status = MmAllocatePhysicalAddress(&PhysicalPML4);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    ULONG_PTR virtualPML4 = PagingAllocatePageWithPhysicalAddress(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END, PAGE_PRESENT | PAGE_WRITE, PhysicalPML4);

    RtlZeroMemory((UINT8*)virtualPML4, PAGE_SIZE);

    ((ULONG_PTR*)virtualPML4)[PML4EntryForRecursivePaging] = PhysicalPML4 | PAGE_PRESENT | PAGE_WRITE;
    ((ULONG_PTR*)virtualPML4)[KERNEL_DESIRED_PML4_ENTRY] = KernelPml4Entry;

    pOutAddressSpace->TopStructPhysAddress = PhysicalPML4;
    InitializeListHead(&pOutAddressSpace->SectionLinkHead);

    return STATUS_SUCCESS;
}

VOID 
NTAPI
MmApplyAddressSpace(
    PADDRESS_SPACE AddressSpace)
{
    __writecr3(AddressSpace->TopStructPhysAddress);
}


ULONG_PTR PagingFindFreePages(ULONG_PTR min, ULONG_PTR max, SIZE_T count)
{
    UINT64 i;

    if (max < min)
    {
        UINT64 temp;
        temp = max;
        max = min;
        min = temp;
    }

    if ((max & 0xFFFF000000000000) != (min & 0xFFFF000000000000))
    {
        KeBugCheck(0X4D);
    }

    max &= 0xFFFFFFFFFFFF;
    min &= 0xFFFFFFFFFFFF;

    for (i = min / PAGE_SIZE_SMALL; i < max / PAGE_SIZE_SMALL; i++)
    {
        UINT64 j;
        BOOL possible = TRUE;

        if (i + count >= max / PAGE_SIZE_SMALL)
            break;

        for (j = 0; j < count; j++)
        {
            if (PagingCheckIsPageIndexFree(i + j) == FALSE)
            {
                i = i + j;
                possible = FALSE;
                break;
            }
        }

        if (possible)
        {
            return ToCanonicalAddress(i * PAGE_SIZE_SMALL);
        }
    }

    return -1LL;
}

ULONG_PTR PagingAllocatePageWithPhysicalAddress(ULONG_PTR min, ULONG_PTR max, UINT16 flags, ULONG_PTR physPage)
{
    ULONG_PTR result;
    KeAcquireSpinLockAtDpcLevel(&PagingSpinlock);
    result = PagingFindFreePages(min, max, 1);
    PagingMapPage(result, physPage, flags);
    KeReleaseSpinLockFromDpcLevel(&PagingSpinlock);
    return ToCanonicalAddress(result);
}

ULONG_PTR PagingAllocatePageEx(ULONG_PTR min, ULONG_PTR max, UINT16 flags)
{
    ULONG_PTR physAddr;
    PFN_NUMBER pfnNumber;
    NTSTATUS status;

    status = MmAllocatePfn(&pfnNumber);
    if (!NT_SUCCESS(status))
        return -1LL;

    physAddr = PA_FROM_PFN(pfnNumber);
    return PagingAllocatePageWithPhysicalAddress(min, max, flags, physAddr);
}

ULONG_PTR PagingAllocatePageFromRange(ULONG_PTR min, ULONG_PTR max)
{
    UINT16 user = (min < PAGING_KERNEL_SPACE && max < PAGING_KERNEL_SPACE) ? (PAGE_USER) : 0;
    return PagingAllocatePageEx(min, max, PAGE_PRESENT | PAGE_WRITE | user);
}

ULONG_PTR PagingAllocatePage()
{
    return PagingAllocatePageFromRange(0, (PML4EntryForRecursivePaging - 1) * PDP_COVERED_SIZE - 1);
}

NTSTATUS PagingMapPage(ULONG_PTR v, ULONG_PTR p, UINT16 f)
{
    ULONG_PTR allocatedPhysAddr;
    PFN_NUMBER allocatedPfn;
    NTSTATUS status;
    UINT64 ptIndex, pdIndex, pdpIndex, pml4Index;
    UINT64* cRecursivePagingPML4Base = RecursivePagingPML4Base;
    UINT64* cRecursivePagingPDPsBase = RecursivePagingPDPsBase;
    UINT64* cRecursivePagingPDsBase = RecursivePagingPDsBase;
    UINT64* cRecursivePagingPTsBase = RecursivePagingPTsBase;
    v = PAGE_ALIGN(v);
    p = PAGE_ALIGN(p);
    v &= 0xFFFFFFFFFFFF; /* ignore bits 63-48, they are just a sign extension */

    ptIndex = v >> 12;
    pdIndex = v >> 21;
    pdpIndex = v >> 30;
    pml4Index = v >> 39;

    if (!(cRecursivePagingPML4Base[pml4Index]))
    {
        status = MmAllocatePfn(&allocatedPfn);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
        allocatedPhysAddr = PA_FROM_PFN(allocatedPfn);
        cRecursivePagingPML4Base[pml4Index] = allocatedPhysAddr | (PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        PagingTLBFlushPage((UINT64)cRecursivePagingPDPsBase + 512 * pml4Index);
        RtlZeroMemory(cRecursivePagingPDPsBase + 512 * pml4Index, PAGE_SIZE_SMALL);
    }

    if (!(cRecursivePagingPDPsBase[pdpIndex]))
    {
        status = MmAllocatePfn(&allocatedPfn);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
        allocatedPhysAddr = PA_FROM_PFN(allocatedPfn);
        cRecursivePagingPDPsBase[pdpIndex] = allocatedPhysAddr | (PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        PagingTLBFlushPage((UINT64)cRecursivePagingPDsBase + 512 * pdpIndex);
        RtlZeroMemory(cRecursivePagingPDsBase + 512 * pdpIndex, PAGE_SIZE_SMALL);
    }

    if (!(cRecursivePagingPDsBase[pdIndex]))
    {
        status = MmAllocatePfn(&allocatedPfn);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
        allocatedPhysAddr = PA_FROM_PFN(allocatedPfn);
        cRecursivePagingPDsBase[pdIndex] = allocatedPhysAddr | (PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        PagingTLBFlushPage((UINT64)cRecursivePagingPTsBase + 512 * pdIndex);
        RtlZeroMemory(cRecursivePagingPTsBase + 512 * pdIndex, PAGE_SIZE_SMALL);
    }

    cRecursivePagingPTsBase[ptIndex] = ((UINT64)p) | f;

    if (pml4Index == 510)
    {
        PagingTLBFlush();
    }
    else
    {
        PagingTLBFlushPage((UINT64)v);
    }

    return STATUS_SUCCESS;
}

ULONG_PTR
PagingGetTableMapping(
    ULONG_PTR virtualAddress)
{
    ULONG_PTR ptIndex, pdIndex, pdpIndex, pml4Index, vUINT;
    UINT64* const cRecursivePagingPML4Base = RecursivePagingPML4Base;
    UINT64* const cRecursivePagingPDPsBase = RecursivePagingPDPsBase;
    UINT64* const cRecursivePagingPDsBase = RecursivePagingPDsBase;
    UINT64* const cRecursivePagingPTsBase = RecursivePagingPTsBase;
    vUINT = (UINT64)virtualAddress; /* used to avoid annoying type casts */
    vUINT &= 0xFFFFFFFFFFFF; /* ignore bits 63-48, they are just a sign extension */

    ptIndex = vUINT >> 12;
    pdIndex = vUINT >> 21;
    pdpIndex = vUINT >> 30;
    pml4Index = vUINT >> 39;

    if (!(cRecursivePagingPML4Base[pml4Index]))
    {
        return 0;
    }

    if (!(cRecursivePagingPDPsBase[pdpIndex]))
    {
        return 0;
    }

    if (!(cRecursivePagingPDsBase[pdIndex]))
    {
        return 0;
    }

    return (ULONG_PTR)cRecursivePagingPTsBase[ptIndex];
}

VOID SetupCR4()
{
    ULONG64 CR4 = __readcr4();
    CR4 &= (~4096); /* ensure level 5 paging is disabled */
    __writecr4(CR4);
}

VOID
NTAPI
PagingEnableSystemWriteProtection(VOID)
{
    ULONG64 CR0 = __readcr0();
    CR0 |= 65536;
    __writecr0(CR0);
}

VOID
NTAPI
PagingDisableSystemWriteProtection(VOID)
{
    ULONG64 CR0 = __readcr0();
    CR0 &= (~65536);
    __writecr0(CR0);
}

VOID SetupCR0()
{
    PagingEnableSystemWriteProtection();
}

ULONG_PTR PhysicalAddressFunctionAllocPages(ULONG_PTR irrelevant, ULONG_PTR relativeIrrelevant)
{
    NTSTATUS status;
    ULONG_PTR physPage;

    status = MmAllocatePhysicalAddress(&physPage);
    if (!NT_SUCCESS(status))
        return -1LL;

    return physPage;
}

NTSTATUS
static
PagingMapPageBeforeInit(
    UINT64* PML4,
    ULONG_PTR p,
    ULONG_PTR v,
    USHORT f)
{
    ULONG_PTR allocatedPhysAddr;
    PFN_NUMBER allocatedPfn;
    NTSTATUS status;
    UINT64 ptIndex, pdIndex, pdpIndex, pml4Index;
    PULONG_PTR Pdp, Pd, Pt;
    v = PAGE_ALIGN(v);
    p = PAGE_ALIGN(p);
    v &= 0xFFFFFFFFFFFF;

    ptIndex = (v >> 12) % 512;
    pdIndex = (v >> 21) % 512;
    pdpIndex = (v >> 30) % 512;
    pml4Index = (v >> 39) % 512;

    if (!(PML4[pml4Index]))
    {
        status = MmAllocatePfn(&allocatedPfn);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
        allocatedPhysAddr = PA_FROM_PFN(allocatedPfn);
        PML4[pml4Index] = allocatedPhysAddr;
        RtlZeroMemory((PVOID)PML4[pml4Index], PAGE_SIZE_SMALL);
        PML4[pml4Index] |= (PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    Pdp = (PULONG_PTR)PAGE_ALIGN(PML4[pml4Index]);

    if (!(Pdp[pdpIndex]))
    {
        status = MmAllocatePfn(&allocatedPfn);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
        allocatedPhysAddr = PA_FROM_PFN(allocatedPfn);
        Pdp[pdpIndex] = allocatedPhysAddr;
        RtlZeroMemory((PVOID)Pdp[pdpIndex], PAGE_SIZE_SMALL);
        Pdp[pdpIndex] |= (PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    Pd = (PULONG_PTR)PAGE_ALIGN(Pdp[pdpIndex]);

    if (!(Pd[pdIndex]))
    {
        status = MmAllocatePfn(&allocatedPfn);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
        allocatedPhysAddr = PA_FROM_PFN(allocatedPfn);
        Pd[pdIndex] = allocatedPhysAddr;
        RtlZeroMemory((PVOID)Pd[pdIndex], PAGE_SIZE_SMALL);
        Pd[pdIndex] |= (PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    Pt = (PULONG_PTR)PAGE_ALIGN(Pd[pdIndex]);
    Pt[ptIndex] = ((UINT64)p) | f;

    return STATUS_SUCCESS;
}

static
NTSTATUS
PagingMapRangeBeforeInit(
    ULONG_PTR* PML4,
    ULONG_PTR PhysicalStart,
    ULONG_PTR PhysicalEnd,
    ULONG_PTR VirtualStart,
    USHORT Flags)
{
    ULONG_PTR AddressDelta = 0;
    NTSTATUS Status;

    for (;
        AddressDelta + PhysicalStart <= PhysicalEnd;
        AddressDelta += PAGE_SIZE)
    {
        Status = PagingMapPageBeforeInit(
            PML4,
            PhysicalStart + AddressDelta,
            VirtualStart + AddressDelta,
            Flags);

        if (!NT_SUCCESS(Status))
        {
            return Status;
        }
    }

    return STATUS_SUCCESS;
}

/* FIXME: This way of mapping kernel is ridiculous.
 * Create some way of mapping pages in pre-paging-enabled enviroment. */
NTSTATUS PagingInit(
    ULONG_PTR MinKernelAddress,
    ULONG_PTR MaxKernelAddress)
{
    ULONG_PTR *pml4, rsp;
    ULONG_PTR physAddr;
    NTSTATUS status;

    SetupCR0();
    SetupCR4();

    /* Get an address for some stack allocated memory */
    rsp = (ULONG_PTR)_alloca(1);

    status = MmAllocatePhysicalAddress(&physAddr);
    if (!NT_SUCCESS(status))
        return status;

    pml4 = (UINT64*)physAddr;
    RtlZeroMemory(pml4, PAGE_SIZE_SMALL);

    /* Identitty map the temporary kernel location. */
    PagingMapRangeBeforeInit(
        pml4,
        MinKernelAddress,
        MaxKernelAddress,
        MinKernelAddress,
        PAGE_PRESENT | PAGE_WRITE);

    /* Identity map the old kernel stack. */
    PagingMapRangeBeforeInit(
        pml4,
        PAGE_ALIGN(rsp) - 10 * PAGE_SIZE,
        PAGE_ALIGN(rsp) + 1 * PAGE_SIZE,
        PAGE_ALIGN(rsp) - 10 * PAGE_SIZE,
        PAGE_PRESENT | PAGE_WRITE);

    /* Map kernel to the high memory. */
    PagingMapRangeBeforeInit(
        pml4,
        MinKernelAddress,
        MaxKernelAddress,
        KERNEL_DESIRED_LOCATION,
        PAGE_PRESENT | PAGE_WRITE);

    ULONG_PTR MemMapPageSize =
        (NumberOfPfnEntries * sizeof(MMPFN_ENTRY) + PAGE_SIZE);

    /* For physical memory map */
    PagingMapRangeBeforeInit(
        pml4,
        PAGE_ALIGN(PfnEntries),
        PAGE_ALIGN(PfnEntries) + MemMapPageSize,
        KERNEL_DESIRED_LOCATION + (MaxKernelAddress - MinKernelAddress),
        PAGE_PRESENT | PAGE_WRITE);

    MiFlagPfnsForRemap();

    PfnEntries = (PMMPFN_ENTRY)
        ((KERNEL_DESIRED_LOCATION + (MaxKernelAddress - MinKernelAddress)) |
            ((ULONG_PTR)PfnEntries & (~PAGE_ADDRESS_MASK)));

    /* For recursive paging */
    pml4[PML4EntryForRecursivePaging] = ((UINT64)pml4) | (PAGE_WRITE | PAGE_PRESENT);
    KernelPml4Entry = pml4[KERNEL_DESIRED_PML4_ENTRY];

    __writecr3((UINT64)pml4);
    MmReinitPhysAllocator(PfnEntries, NumberOfPfnEntries);
    return STATUS_SUCCESS;
}

VOID PagingMapAndInitFramebuffer()
{
    UINT64 i;
    NTSTATUS status;

    for (i = 0; i < FrameBufferSize() / PAGE_SIZE_SMALL + 1; i++)
    {
        status = PagingMapPage(FRAMEBUFFER_DESIRED_LOCATION + i * PAGE_SIZE_SMALL, ((UINT64)gFramebuffer) + i * PAGE_SIZE_SMALL, 0x3);
    }

    TextIoInitialize(
        (UINT32*)FRAMEBUFFER_DESIRED_LOCATION,
        (UINT32*)(FRAMEBUFFER_DESIRED_LOCATION + FrameBufferSize()),
        gWidth,
        gHeight,
        gPixelsPerScanline
    );

    TextIoSetColorInformation(0xFFFFFFFF, 0x00000000, 0);

    gFramebuffer = (UINT32*)FRAMEBUFFER_DESIRED_LOCATION;
    gFramebufferEnd = (UINT32*)FRAMEBUFFER_DESIRED_LOCATION + FrameBufferSize();
    TextIoClear();
}

ULONG_PTR PagingAllocatePageBlockWithPhysicalAddresses(SIZE_T n, ULONG_PTR min, ULONG_PTR max, UINT16 flags, ULONG_PTR physFirstPage)
{
    UINT64 i;
    ULONG_PTR virt;

    KeAcquireSpinLockAtDpcLevel(&PagingSpinlock);
    virt = PagingFindFreePages(min, max, n);
    for (i = 0; i < n; i++)
    {
        PagingMapPage((ULONG_PTR)virt + i * PAGE_SIZE_SMALL, (ULONG_PTR)physFirstPage + i * PAGE_SIZE_SMALL, flags);
    }
    KeReleaseSpinLockFromDpcLevel(&PagingSpinlock);

    return virt;
}

ULONG_PTR PagingAllocatePageBlockEx(SIZE_T n, ULONG_PTR min, ULONG_PTR max, UINT16 flags)
{
    UINT64 i;
    ULONG_PTR virt;

    KeAcquireSpinLockAtDpcLevel(&PagingSpinlock);
    virt = PagingFindFreePages(min, max, n);
    for (i = 0; i < n; i++)
    {
        NTSTATUS status;
        ULONG_PTR p;
        status = MmAllocatePhysicalAddress(&p);
        if (!NT_SUCCESS(status))
        {
            KeReleaseSpinLockFromDpcLevel(&PagingSpinlock);
            return -1LL;
        }
        PagingMapPage((ULONG_PTR)virt + i * PAGE_SIZE_SMALL, p, flags);
    }
    KeReleaseSpinLockFromDpcLevel(&PagingSpinlock);

    return virt;
}

ULONG_PTR PagingAllocatePageBlock(SIZE_T n, UINT16 flags)
{
    return PagingAllocatePageBlockEx(n, 0, (1ULL << 47ULL) - PAGE_SIZE_SMALL, flags);
}

ULONG_PTR PagingAllocatePageBlockFromRange(SIZE_T n, ULONG_PTR min, ULONG_PTR max)
{
    return PagingAllocatePageBlockEx(n, min, max, PAGE_PRESENT | PAGE_WRITE);
}

ULONG_PTR PagingMapStrcutureToVirtual(ULONG_PTR physicalAddress, SIZE_T structureSize, UINT16 flags)
{
    ULONG_PTR virt = (ULONG_PTR)PagingAllocatePageBlockWithPhysicalAddresses(
        (PAGE_ALIGN(physicalAddress + structureSize) - PAGE_ALIGN(physicalAddress)) / PAGE_SIZE_SMALL + 1,
        PAGING_KERNEL_SPACE,
        PAGING_KERNEL_SPACE_END,
        flags,
        PAGE_ALIGN(physicalAddress)
    );

    ULONG_PTR delta = physicalAddress - PAGE_ALIGN(physicalAddress);
    return virt + delta;
}
