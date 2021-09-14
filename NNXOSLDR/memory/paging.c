#include "paging.h"
#include "video/SimpleTextIo.h"
#include "physical_allocator.h"
#include "MemoryOperations.h"

/*
	Virtual memory layout:

	PML4:
	PDPs 0-509 : pages
	PDP  510   : paging structures
	PDP  511   : kernel space

	Thus, the memory layout looks like this (fig. not to scale, of course):

	=================================================================================================================================================================================================
	| 510 * PDP_COVERED_SIZE | 510 * PD_COVERED_SIZE | 510 * PT_COVERED_SIZE | 510 * PAGE_SIZE_SMALL |  1 * PAGE_SIZE_SMALL  | 1 * PAGE_SIZE_SMALL | 1 * PT_COVERED_SIZE | 1 * PD_COVERED_SIZE | 1 * PDP_COVERED_SIZE |
	|     bytes of Pages     |     bytes of PTs      |    bytes of PDs       |  bytes of PDPs  | bytes of a PML4 |  bytes of PDP |     bytes of PDs    |     bytes of PTs    |  bytes of Pages      |
	=================================================================================================================================================================================================
*/

#define RecursivePagingPagesSizePreBoundary		((UINT64*)ToCanonicalAddress(PDP_COVERED_SIZE * PML4EntryForRecursivePaging))

#define RecursivePagingPTsBase					((UINT64*)ToCanonicalAddress(RecursivePagingPagesSizePreBoundary))
#define RecursivePagingPTsSizePreBoundary		PD_COVERED_SIZE * PML4EntryForRecursivePaging

#define RecursivePagingPDsBase					((UINT64*)ToCanonicalAddress(((UINT64)RecursivePagingPTsBase) + RecursivePagingPTsSizePreBoundary))
#define RecursivePagingPDsSizePreBoundary		PT_COVERED_SIZE * PML4EntryForRecursivePaging

#define RecursivePagingPDPsBase					((UINT64*)ToCanonicalAddress(((UINT64)RecursivePagingPDsBase) + RecursivePagingPDsSizePreBoundary))
#define RecursivePagingPDPsSizePreBoundary		PAGE_SIZE_SMALL * PML4EntryForRecursivePaging

#define RecursivePagingPML4Base					((UINT64*)ToCanonicalAddress(((UINT64)RecursivePagingPDPsBase) + RecursivePagingPDPsSizePreBoundary))
#define RecursivePagingPML4Size					PAGE_SIZE_SMALL

extern BOOL gInteruptInitialized;

VOID PagingTLBFlushPage(UINT64 page)
{
	/* TODO: inform other processors if neccessary */
	PagingInvalidatePage(ToCanonicalAddress(page));
}

VOID* PagingAllocatePageWithPhysicalAddress(UINT64 min, UINT64 max, UINT16 flags, VOID* physPage)
{
	UINT64 i, result = (UINT64)-1LL;
	min &= 0xFFFFFFFFFFFF;
	max &= 0xFFFFFFFFFFFF;

	for (i = min / PAGE_SIZE_SMALL; i < max / PAGE_SIZE_SMALL; i++)
{
		if ((RecursivePagingPML4Base[i / (512 * 512 * 512)]) == 0)
{
			result = i;
			break;
		}
		if ((RecursivePagingPDPsBase[i / (512 * 512)]) == 0)
{
			result = i;
			break;
		}
		if ((RecursivePagingPDsBase[i / 512]) == 0)
{
			result = i;
			break;
		}

		if ((RecursivePagingPTsBase[i]) == 0)
{
			result = i;
			break;
		}
	}

	PagingMapPage(result * PAGE_SIZE_SMALL, physPage, flags);
	return ToCanonicalAddress(result * PAGE_SIZE_SMALL);
}

VOID* PagingAllocatePageEx(UINT64 min, UINT64 max, UINT16 flags, UINT8 physMemType)
{
	return PagingAllocatePageWithPhysicalAddress(min, max, flags, InternalAllocatePhysicalPageWithType(physMemType));
}

VOID* PagingAllocatePageFromRange(UINT64 min, UINT64 max)
{
	return PagingAllocatePageEx(min, max, PAGE_PRESENT | PAGE_WRITE, MEM_TYPE_USED);
}

VOID* PagingAllocatePage()
{
	return PagingAllocatePageFromRange(0, (PML4EntryForRecursivePaging - 1) * PDP_COVERED_SIZE - 1);
}

VOID PagingMapPage(VOID* v, VOID* p, UINT16 f)
{
	UINT64 ptIndex, pdIndex, pdpIndex, pml4Index, vUINT;
	UINT64* const cRecursivePagingPML4Base = RecursivePagingPML4Base;
	UINT64* const cRecursivePagingPDPsBase = RecursivePagingPDPsBase;
	UINT64* const cRecursivePagingPDsBase = RecursivePagingPDsBase;
	UINT64* const cRecursivePagingPTsBase = RecursivePagingPTsBase;
	v = PAGE_ALIGN(v);
	p = PAGE_ALIGN(p);
	vUINT = v; /* used to avoid annoying type casts */
	vUINT &= 0xFFFFFFFFFFFF; /* ignore bits 63-48, they are just a sign extension */

	ptIndex = vUINT >> 12;
	pdIndex = vUINT >> 21;
	pdpIndex = vUINT >> 30;
	pml4Index = vUINT >> 39;

	if (!(cRecursivePagingPML4Base[pml4Index]))
{
		cRecursivePagingPML4Base[pml4Index] = ((UINT64)InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM)) | (PAGE_PRESENT | PAGE_WRITE);
		PagingTLBFlushPage(cRecursivePagingPDPsBase + 512 * pml4Index);
		MemSet(cRecursivePagingPDPsBase + 512 * pml4Index, 0, PAGE_SIZE_SMALL);
	}

	if (!(cRecursivePagingPDPsBase[pdpIndex]))
{
		cRecursivePagingPDPsBase[pdpIndex] = ((UINT64)InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM)) | (PAGE_PRESENT | PAGE_WRITE);
		PagingTLBFlushPage(cRecursivePagingPDsBase + 512 * pdpIndex);
		MemSet(cRecursivePagingPDsBase + 512 * pdpIndex, 0, PAGE_SIZE_SMALL);
	}

	if (!(cRecursivePagingPDsBase[pdIndex]))
{
		cRecursivePagingPDsBase[pdIndex] = ((UINT64)InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM)) | (PAGE_PRESENT | PAGE_WRITE);
		PagingTLBFlushPage(cRecursivePagingPTsBase + 512 * pdIndex);
		MemSet(cRecursivePagingPTsBase + 512 * pdIndex, 0, PAGE_SIZE_SMALL);
	}

	if (cRecursivePagingPTsBase[ptIndex])
{
		if (gInteruptInitialized)
{
			PrintT("Warning: page overriden %x to %x\n", cRecursivePagingPTsBase[ptIndex], ((UINT64)p) | f);
		}
	}

	cRecursivePagingPTsBase[ptIndex] = ((UINT64)p) | f;

	if (pml4Index == 510)
{
		PagingTLBFlush();
	}
	else
{
		PagingTLBFlushPage(v);
	}
}

VOID SetupCR4()
{
	UINT64 CR4 = GetCR4();
	CR4 &= (~4096); /* ensure level 5 paging is disabled */
	SetCR4(CR4);
}

VOID SetupCR0()
{
	UINT64 CR0 = GetCR0();
	CR0 &= (~65536);
	SetCR0(CR0);
}

UINT64 PhysicalAddressFunctionAllocPages(UINT64 irrelevant, UINT64 relativeIrrelevant, UINT8 physMemoryType)
{
	return InternalAllocatePhysicalPageWithType(physMemoryType);
}

UINT64 PhysicalAddressFunctionIdentify(UINT64 a, UINT64 b, UINT8 physMemoryType)
{
	return a;
}

UINT64 PhysicalAddressFunctionGlobalMemoryMap(UINT64 address, UINT64 relativeAddrss, UINT8 physMemoryType)
{
	return PAGE_ALIGN((UINT64)GlobalPhysicalMemoryMap) + relativeAddrss;
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
		PML4[PML4Index] = InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM);
		MemSet(PML4[PML4Index], 0, PAGE_SIZE_SMALL);
		PML4[PML4Index] |= (PAGE_WRITE | PAGE_PRESENT);
	}

	PDPTempKernel = PAGE_ALIGN(PML4[PML4Index]);

	if ((PDPTempKernel[PDPIndex] & PAGE_PRESENT) == 0)
{
		PDPTempKernel[PDPIndex] = InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM);
		MemSet(PDPTempKernel[PDPIndex], 0, PAGE_SIZE_SMALL);
		PDPTempKernel[PDPIndex] |= (PAGE_WRITE | PAGE_PRESENT);
	}
	
	PDTempKernel = PAGE_ALIGN(PDPTempKernel[PDPIndex]);

	/* allocate page tables */
	for (i = initialPageTable; i < howManyPageTables + initialPageTable; i++)
{
		PDTempKernel[i] = InternalAllocatePhysicalPageWithType(physMemoryType);
		for (j = 0; j < 512; j++)
{
			((UINT64*)PDTempKernel[i])[j] = PhysicalAddressFunction(
				ToCanonicalAddress(((i + (512 * PML4Index + PDPIndex) * 512) * 512 + j) << 12), 
				((i - initialPageTable) * 512 + j) << 12,
				physMemoryType
			) 
				| (PAGE_PRESENT | PAGE_WRITE | additionalFlags);
		}

		PDTempKernel[i] |= (PAGE_PRESENT | PAGE_WRITE);
	}
}

const UINT64 loaderTemporaryKernelPTStart = 0;
const UINT64 loaderTemporaryKernelPTSize = 32;

/*	TODO: Map ACPI structures, so they're always accessible 
	(and mark them as used in the GlobalPhysicalMemoryMap) 
	
	Perhaps it should be done on structure first use, ex.
	when the OS retrieves the RDSP it should map it, and set
	the pointer to the updated value. Later, upon (X/R)SDT's
	first usage, map them, change their pointer in the RDSP
	(ACPI doesn't use the tables once it creates them, right?? right??)*/
VOID PagingInit()
{
	UINT64 i, *PML4, const pageTableKernelReserve = 16, RSP;
	const UINT64 virtualGlobalPhysicalMemoryMapSize = ToCanonicalAddress((KERNEL_PML4_ENTRY * 512 * 512 + pageTableKernelReserve) * 512 * 4096);
	SetupCR0();
	SetupCR4();
	RSP = GetRSP();

	PML4 = InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM);
	MemSet(PML4, 0, PAGE_SIZE_SMALL);

	/* for loader-temporary kernel */
	InternalPagingAllocatePagingStructures(PML4, 0, 0, loaderTemporaryKernelPTStart, loaderTemporaryKernelPTSize, PhysicalAddressFunctionIdentify, 0, MEM_TYPE_USED);

	/* for stack */
	InternalPagingAllocatePagingStructures(PML4, 0, 0, RSP / 4096 / 512 - 2, 3, PhysicalAddressFunctionIdentify, 0, MEM_TYPE_USED_PERM);

	/* for main kernel */
	InternalPagingAllocatePagingStructures(PML4, KERNEL_PML4_ENTRY, 0, 0, pageTableKernelReserve, PhysicalAddressFunctionAllocPages, PAGE_GLOBAL, MEM_TYPE_USED_PERM);

	/* for physical memory map */
	InternalPagingAllocatePagingStructures(PML4, KERNEL_PML4_ENTRY, 0, pageTableKernelReserve,
		GlobalPhysicalMemoryMapSize / (4096 * 512) + 2, PhysicalAddressFunctionGlobalMemoryMap, 0, MEM_TYPE_USED_PERM);

	/* for recursive paging */
	PML4[PML4EntryForRecursivePaging] = ((UINT64)PML4) | (PAGE_WRITE | PAGE_PRESENT);

	SetCR3(PML4);
	GlobalPhysicalMemoryMap = virtualGlobalPhysicalMemoryMapSize | (((UINT64)GlobalPhysicalMemoryMap) & 0xFFF);

	for (i = 0; i < FrameBufferSize() / PAGE_SIZE_SMALL + 1; i++)
{
		PagingMapPage(FRAMEBUFFER_DESIRED_LOCATION + i * PAGE_SIZE_SMALL, ((UINT64)gFramebuffer) + i * PAGE_SIZE_SMALL, 0x3);
	}

	TextIoInitialize(FRAMEBUFFER_DESIRED_LOCATION, FRAMEBUFFER_DESIRED_LOCATION + FrameBufferSize(), 0, 0, 0);
	TextIoSetColorInformation(0xFFFFFFFF, 0x00000000, 0);
}

VOID PagingKernelInit()
{
	UINT64 i;

	for (i = loaderTemporaryKernelPTStart; i < loaderTemporaryKernelPTSize + loaderTemporaryKernelPTStart; i++)
{
		//PagingMapPage(RecursivePagingPTsBase + i * 512, 0, 0);
	}
	PagingTLBFlush();

}
