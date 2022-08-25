#include "../paging.h"
#include <SimpleTextIo.h>
#include <HAL/physical_allocator.h>
#include <MemoryOperations.h>
#include <HAL/spinlock.h>
#include "registers.h"
#include <bugcheck.h>

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

#define RecursivePagingPagesSizePreBoundary        ((UINT64*)ToCanonicalAddress(PDP_COVERED_SIZE * PML4EntryForRecursivePaging))

#define RecursivePagingPTsBase                    ((UINT64*)ToCanonicalAddress((UINT64)RecursivePagingPagesSizePreBoundary))
#define RecursivePagingPTsSizePreBoundary        PD_COVERED_SIZE * PML4EntryForRecursivePaging

#define RecursivePagingPDsBase                    ((UINT64*)ToCanonicalAddress((UINT64)RecursivePagingPTsBase + (UINT64)RecursivePagingPTsSizePreBoundary))
#define RecursivePagingPDsSizePreBoundary        PT_COVERED_SIZE * PML4EntryForRecursivePaging

#define RecursivePagingPDPsBase                    ((UINT64*)ToCanonicalAddress((UINT64)RecursivePagingPDsBase + (UINT64)RecursivePagingPDsSizePreBoundary))
#define RecursivePagingPDPsSizePreBoundary        PAGE_SIZE_SMALL * PML4EntryForRecursivePaging

#define RecursivePagingPML4Base                    ((UINT64*)ToCanonicalAddress((UINT64)RecursivePagingPDPsBase + (UINT64)RecursivePagingPDPsSizePreBoundary))
#define RecursivePagingPML4Size                    PAGE_SIZE_SMALL

KSPIN_LOCK PagingSpinlock;

ULONG_PTR KernelPml4Entry = NULL;

VOID PagingTLBFlushPage(UINT64 page)
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

ULONG_PTR PagingCreateAddressSpace()
{
    ULONG_PTR physicalPML4 = (ULONG_PTR) InternalAllocatePhysicalPage();
    ULONG_PTR virtualPML4 = PagingAllocatePageWithPhysicalAddress(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END, PAGE_PRESENT | PAGE_WRITE, physicalPML4);

    MemSet((UINT8*) virtualPML4, 0, PAGE_SIZE);

    ((ULONG_PTR*) virtualPML4)[PML4EntryForRecursivePaging] = physicalPML4 | PAGE_PRESENT | PAGE_WRITE;
    ((ULONG_PTR*)virtualPML4)[KERNEL_DESIRED_PML4_ENTRY] = KernelPml4Entry;

    return physicalPML4;
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
    KIRQL irql;
    ULONG_PTR result;
    KeAcquireSpinLock(&PagingSpinlock, &irql);
    result = PagingFindFreePages(min, max, 1);
    PagingMapPage(result, physPage, flags);
    KeReleaseSpinLock(&PagingSpinlock, irql);
    return ToCanonicalAddress(result);
}

ULONG_PTR PagingAllocatePageEx(ULONG_PTR min, ULONG_PTR max, UINT16 flags, UINT8 physMemType)
{
    return PagingAllocatePageWithPhysicalAddress(min, max, flags, (ULONG_PTR)InternalAllocatePhysicalPageWithType(physMemType));
}

ULONG_PTR PagingAllocatePageFromRange(ULONG_PTR min, ULONG_PTR max)
{
    UINT16 user = (min < PAGING_KERNEL_SPACE && max < PAGING_KERNEL_SPACE) ? (PAGE_USER) : 0;
    return PagingAllocatePageEx(min, max, PAGE_PRESENT | PAGE_WRITE | user, MEM_TYPE_USED);
}

ULONG_PTR PagingAllocatePage()
{
    return PagingAllocatePageFromRange(0, (PML4EntryForRecursivePaging - 1) * PDP_COVERED_SIZE - 1);
}

VOID PagingMapPage(ULONG_PTR v, ULONG_PTR p, UINT16 f)
{
    UINT64 ptIndex, pdIndex, pdpIndex, pml4Index, vUINT;
    UINT64* const cRecursivePagingPML4Base = RecursivePagingPML4Base;
    UINT64* const cRecursivePagingPDPsBase = RecursivePagingPDPsBase;
    UINT64* const cRecursivePagingPDsBase = RecursivePagingPDsBase;
    UINT64* const cRecursivePagingPTsBase = RecursivePagingPTsBase;
    v = PAGE_ALIGN(v);
    p = PAGE_ALIGN(p);
    vUINT = (UINT64) v; /* used to avoid annoying type casts */
    vUINT &= 0xFFFFFFFFFFFF; /* ignore bits 63-48, they are just a sign extension */

    ptIndex = vUINT >> 12;
    pdIndex = vUINT >> 21;
    pdpIndex = vUINT >> 30;
    pml4Index = vUINT >> 39;

    if (!(cRecursivePagingPML4Base[pml4Index]))
    {
        cRecursivePagingPML4Base[pml4Index] = ((UINT64) InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM)) | (PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        PagingTLBFlushPage((UINT64)cRecursivePagingPDPsBase + 512 * pml4Index);
        MemSet(cRecursivePagingPDPsBase + 512 * pml4Index, 0, PAGE_SIZE_SMALL);
    }

    if (!(cRecursivePagingPDPsBase[pdpIndex]))
    {
        cRecursivePagingPDPsBase[pdpIndex] = ((UINT64) InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM)) | (PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        PagingTLBFlushPage((UINT64) cRecursivePagingPDsBase + 512 * pdpIndex);
        MemSet(cRecursivePagingPDsBase + 512 * pdpIndex, 0, PAGE_SIZE_SMALL);
    }

    if (!(cRecursivePagingPDsBase[pdIndex]))
    {
        cRecursivePagingPDsBase[pdIndex] = ((UINT64) InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM)) | (PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        PagingTLBFlushPage((UINT64) cRecursivePagingPTsBase + 512 * pdIndex);
        MemSet(cRecursivePagingPTsBase + 512 * pdIndex, 0, PAGE_SIZE_SMALL);
    }

    cRecursivePagingPTsBase[ptIndex] = ((UINT64) p) | f;

    if (pml4Index == 510)
    {
        PagingTLBFlush();
    }
    else
    {
        PagingTLBFlushPage((UINT64)v);
    }
}

ULONG_PTR 
PagingGetCurrentMapping(
    ULONG_PTR virtualAddress
)
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
    UINT64 CR4 = GetCR4();
    CR4 &= (~4096); /* ensure level 5 paging is disabled */
    SetCR4(CR4);
}

VOID EnableWriteProtect()
{
    UINT64 CR0 = GetCR0();
    CR0 |= 65536;
    SetCR0(CR0);
}

VOID DisableWriteProtect()
{
    UINT64 CR0 = GetCR0();
    CR0 &= (~65536);
    SetCR0(CR0);
}

VOID SetupCR0()
{
    EnableWriteProtect();
}

ULONG_PTR PhysicalAddressFunctionAllocPages(ULONG_PTR irrelevant, ULONG_PTR relativeIrrelevant, UINT8 physMemoryType)
{
    return (ULONG_PTR)InternalAllocatePhysicalPageWithType(physMemoryType);
}

ULONG_PTR PhysicalAddressFunctionIdentify(ULONG_PTR a, ULONG_PTR b, UINT8 physMemoryType)
{
    return a;
}

ULONG_PTR PhysicalAddressFunctionKernel(ULONG_PTR a, ULONG_PTR b, UINT8 physMemoryType)
{
    return KERNEL_INITIAL_ADDRESS + b;
}

extern PBYTE GlobalPhysicalMemoryMap;
extern UINT64 GlobalPhysicalMemoryMapSize;

UINT64 PhysicalAddressFunctionGlobalMemoryMap(ULONG_PTR address, ULONG_PTR relativeAddrss, UINT8 physMemoryType)
{
    return PAGE_ALIGN((ULONG_PTR) GlobalPhysicalMemoryMap) + relativeAddrss;
}


VOID InternalPagingAllocatePagingStructures(
    UINT64* PML4, UINT64 PML4Index, UINT64 PDPIndex,
    UINT64 initialPageTable, UINT64 howManyPageTables,
    UINT64(*PhysicalAddressFunction)(UINT64, UINT64, UINT8),
    UINT16 additionalFlags,
    UINT8 physMemoryType)
{
    UINT64 i, j, *PDPTempKernel, *PDTempKernel;
    if ((PML4[PML4Index] & PAGE_PRESENT) == 0)
    {
        PML4[PML4Index] = (UINT64) InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM);
        MemSet((PVOID)PML4[PML4Index], 0, PAGE_SIZE_SMALL);
        PML4[PML4Index] |= (PAGE_WRITE | PAGE_PRESENT | PAGE_USER);
    }

    PDPTempKernel = (UINT64*) PAGE_ALIGN(PML4[PML4Index]);

    if ((PDPTempKernel[PDPIndex] & PAGE_PRESENT) == 0)
    {
        PDPTempKernel[PDPIndex] = (UINT64) InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM);
        MemSet((PVOID) PDPTempKernel[PDPIndex], 0, PAGE_SIZE_SMALL);
        PDPTempKernel[PDPIndex] |= (PAGE_WRITE | PAGE_PRESENT | PAGE_USER);
    }

    PDTempKernel = (UINT64*) PAGE_ALIGN(PDPTempKernel[PDPIndex]);

    /* allocate page tables */
    for (i = initialPageTable; i < howManyPageTables + initialPageTable; i++)
    {
        PDTempKernel[i] = (UINT64) InternalAllocatePhysicalPageWithType(physMemoryType);
        for (j = 0; j < 512; j++)
        {
            ((UINT64*) PDTempKernel[i])[j] = PhysicalAddressFunction(
                ToCanonicalAddress(((i + (512 * PML4Index + PDPIndex) * 512) * 512 + j) << 12),
                ((i - initialPageTable) * 512 + j) << 12,
                physMemoryType
            )
                | (PAGE_PRESENT | PAGE_WRITE | additionalFlags);
        }

        PDTempKernel[i] |= (PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }
}

const UINT64 loaderTemporaryKernelPTStart = 0;
const UINT64 loaderTemporaryKernelPTSize = 32;

VOID PagingInit(PBYTE pbPhysicalMemoryMap, QWORD dwPhysicalMemoryMapSize)
{
    UINT64 *pml4, rsp;
    const UINT64 pageTableKernelReserve = 16;
    const UINT64 virtualGlobalPhysicalMemoryMap = ToCanonicalAddress((KERNEL_DESIRED_PML4_ENTRY * 512 * 512 + pageTableKernelReserve) * 512 * 4096);
    
    GlobalPhysicalMemoryMap = pbPhysicalMemoryMap;
    GlobalPhysicalMemoryMapSize = dwPhysicalMemoryMapSize;
    
    SetupCR0();
    SetupCR4();
    rsp = GetRSP();

    pml4 = (UINT64*)InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM);
    MemSet(pml4, 0, PAGE_SIZE_SMALL);

    /* for loader-temporary kernel */
    InternalPagingAllocatePagingStructures(pml4, 0, 0, loaderTemporaryKernelPTStart, loaderTemporaryKernelPTSize, PhysicalAddressFunctionIdentify, 0, MEM_TYPE_USED);

    /* for stack */
    InternalPagingAllocatePagingStructures(pml4, 0, 0, rsp / PAGE_SIZE_SMALL / 512 - 2, 3, PhysicalAddressFunctionIdentify, 0, MEM_TYPE_USED_PERM);

    /* for main kernel */
    InternalPagingAllocatePagingStructures(pml4, KERNEL_DESIRED_PML4_ENTRY, 0, 0, pageTableKernelReserve, PhysicalAddressFunctionKernel, PAGE_GLOBAL, MEM_TYPE_USED_PERM);

    /* for physical memory map */
    InternalPagingAllocatePagingStructures(pml4, KERNEL_DESIRED_PML4_ENTRY, 0, pageTableKernelReserve,
                                           dwPhysicalMemoryMapSize / (PAGE_SIZE_SMALL * 512) + 2, PhysicalAddressFunctionGlobalMemoryMap, 0, MEM_TYPE_USED_PERM);

    /* for recursive paging */
    pml4[PML4EntryForRecursivePaging] = ((UINT64) pml4) | (PAGE_WRITE | PAGE_PRESENT);
    GlobalPhysicalMemoryMap = (PBYTE)(virtualGlobalPhysicalMemoryMap | (((UINT64) pbPhysicalMemoryMap) & 0xFFF));

    KernelPml4Entry = pml4[KERNEL_DESIRED_PML4_ENTRY];

    SetCR3((UINT64)pml4);
}

VOID PagingMapAndInitFramebuffer()
{
    UINT64 i;

    for (i = 0; i < FrameBufferSize() / PAGE_SIZE_SMALL + 1; i++)
    {
        PagingMapPage(FRAMEBUFFER_DESIRED_LOCATION + i * PAGE_SIZE_SMALL, ((UINT64) gFramebuffer) + i * PAGE_SIZE_SMALL, 0x3);
    }

    TextIoInitialize(
		(UINT32*)FRAMEBUFFER_DESIRED_LOCATION, 
		(UINT32*)(FRAMEBUFFER_DESIRED_LOCATION + FrameBufferSize()), 
		gWidth, 
		gHeight, 
		gPixelsPerScanline);

    TextIoSetColorInformation(0xFFFFFFFF, 0x00000000, 0);

    TextIoClear();

    gFramebuffer = (UINT32*) FRAMEBUFFER_DESIRED_LOCATION;
    gFramebufferEnd = (UINT32*) FRAMEBUFFER_DESIRED_LOCATION + FrameBufferSize();
}

ULONG_PTR PagingAllocatePageBlockWithPhysicalAddresses(SIZE_T n, ULONG_PTR min, ULONG_PTR max, UINT16 flags, ULONG_PTR physFirstPage)
{
    UINT64 i;
    ULONG_PTR virt;
    KIRQL irql;

    KeAcquireSpinLock(&PagingSpinlock, &irql);
    virt = PagingFindFreePages(min, max, n);
    for (i = 0; i < n; i++)
    {
        PagingMapPage((ULONG_PTR)virt + i * PAGE_SIZE_SMALL, (ULONG_PTR)physFirstPage + i * PAGE_SIZE_SMALL, flags);
    }
    KeReleaseSpinLock(&PagingSpinlock, irql);

	return virt;
}

ULONG_PTR PagingAllocatePageBlockEx(SIZE_T n, ULONG_PTR min, ULONG_PTR max, UINT16 flags)
{
    UINT64 i;
    ULONG_PTR virt;
    KIRQL irql;

    KeAcquireSpinLock(&PagingSpinlock, &irql);
    virt = PagingFindFreePages(min, max, n);
    for (i = 0; i < n; i++)
    {
        ULONG_PTR p;
        p = (ULONG_PTR)InternalAllocatePhysicalPageWithType(MEM_TYPE_USED);
        PagingMapPage((ULONG_PTR)virt + i * PAGE_SIZE_SMALL, p, flags);
    }
    KeReleaseSpinLock(&PagingSpinlock, irql);

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
	ULONG_PTR virt = (ULONG_PTR) PagingAllocatePageBlockWithPhysicalAddresses(
		(PAGE_ALIGN(physicalAddress + structureSize) - PAGE_ALIGN(physicalAddress)) / PAGE_SIZE_SMALL + 1,
		PAGING_KERNEL_SPACE,
		PAGING_KERNEL_SPACE_END,
		flags,
		PAGE_ALIGN(physicalAddress));

	ULONG_PTR delta = physicalAddress - PAGE_ALIGN(physicalAddress);
	return virt + delta;
}

ULONG_PTR PagingGetAddressSpace()
{
    return (ULONG_PTR) GetCR3();
}

VOID PagingSetAddressSpace(ULONG_PTR AddressSpace)
{
    SetCR3(AddressSpace);
}