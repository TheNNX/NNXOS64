#include "paging.h"
#include "video/SimpleTextIO.h"
#include "physical_allocator.h"
#include "MemoryOperations.h"

/*
	Virtual memory layout:

	PML4:
	PDPs 0-509 : pages
	PDP  510   : pointer to PML4
	PDP  511   : kernel space

	Thus, the memory layout looks like this (fig. not to scale, of course):

	=================================================================================================================================================================================================
	| 510 * PDP_COVERED_SIZE | 510 * PD_COVERED_SIZE | 510 * PT_COVERED_SIZE | 510 * PAGE_SIZE |  1 * PAGE_SIZE  | 1 * PAGE_SIZE | 1 * PT_COVERED_SIZE | 1 * PD_COVERED_SIZE | 1 * PDP_COVERED_SIZE |
	|     bytes of Pages     |     bytes of PTs      |    bytes of PDs       |  bytes of PDPs  | bytes of a PML4 |  bytes of PDP |     bytes of PDs    |     bytes of PTs    |  bytes of Pages      |
	=================================================================================================================================================================================================
*/

#define KERNEL_PML4_ENTRY 511

#define PML4EntryForRecursivePaging 510ULL

#define RecursivePagingPagesSizePreBoundary		((UINT64*)ToCanonicalAddress(PDP_COVERED_SIZE * PML4EntryForRecursivePaging))

#define RecursivePagingPTsBase					((UINT64*)ToCanonicalAddress(RecursivePagingPagesSizePreBoundary))
#define RecursivePagingPTsSizePreBoundary		PD_COVERED_SIZE * PML4EntryForRecursivePaging

#define RecursivePagingPDsBase					((UINT64*)ToCanonicalAddress(((UINT64)RecursivePagingPTsBase) + RecursivePagingPTsSizePreBoundary))
#define RecursivePagingPDsSizePreBoundary		PT_COVERED_SIZE * PML4EntryForRecursivePaging

#define RecursivePagingPDPsBase					((UINT64*)ToCanonicalAddress(((UINT64)RecursivePagingPDsBase) + RecursivePagingPDsSizePreBoundary))
#define RecursivePagingPDPsSizePreBoundary		PAGE_SIZE * PML4EntryForRecursivePaging

#define RecursivePagingPML4Base					((UINT64*)ToCanonicalAddress(((UINT64)RecursivePagingPDPsBase) + RecursivePagingPDPsSizePreBoundary))
#define RecursivePagingPML4Size					PAGE_SIZE

extern BOOL gInteruptInitialized;

void PagingTLBFlush() {
	SetCR3(GetCR3());
}

void PagingTLBFlushPage(UINT64 page) {
	/* TODO: inform other processors if neccessary */
	PagingInvalidatePage(ToCanonicalAddress(page));
}

void* PagingAllocatePageEx(UINT64 min, UINT64 max, UINT16 flags, UINT8 physMemType) {
	UINT64 i, result = (UINT64)-1LL;
	min &= 0xFFFFFFFFFFFF;
	max &= 0xFFFFFFFFFFFF;

	for (i = min / PAGE_SIZE; i < max / PAGE_SIZE; i++) {
		if ((RecursivePagingPML4Base[i / (512 * 512 * 512)] & PAGE_PRESENT) == 0) {
			result = i;
			break;
		}
		if ((RecursivePagingPDPsBase[i / (512 * 512)] & PAGE_PRESENT) == 0) {
			result = i;
			break;
		}
		if ((RecursivePagingPDsBase[i / 512] & PAGE_PRESENT) == 0) {
			result = i;
			break;
		}

		if ((RecursivePagingPTsBase[i] & PAGE_PRESENT) == 0) {	
			result = i;
			break;
		}
	}

	PagingMapPage(result * PAGE_SIZE, InternalAllocatePhysicalPageWithType(physMemType), flags);
	return ToCanonicalAddress(result * PAGE_SIZE);
}

void* PagingAllocatePageFromRange(UINT64 min, UINT64 max) {
	return PagingAllocatePageEx(min, max, PAGE_PRESENT | PAGE_SYSTEM, MEM_TYPE_KERNEL);
}

void* PagingAllocatePage() {
	return PagingAllocatePageFromRange(0, (PML4EntryForRecursivePaging - 1) * PDP_COVERED_SIZE - 1);
}

void PagingMapPage(void* v, void* p, UINT16 f) {
	UINT64 ptIndex, pdIndex, pdpIndex, pml4Index, vUINT;
	UINT64* const cRecursivePagingPML4Base = RecursivePagingPML4Base;
	UINT64* const cRecursivePagingPDPsBase = RecursivePagingPDPsBase;
	UINT64* const cRecursivePagingPDsBase = RecursivePagingPDsBase;
	UINT64* const cRecursivePagingPTsBase = RecursivePagingPTsBase;
	vUINT = v; /* used to avoid annoying type casts */
	vUINT &= 0xFFFFFFFFFFFF; /* ignore bits 63-48, they are just a sign extension */

	ptIndex = vUINT >> 12;
	pdIndex = vUINT >> 21;
	pdpIndex = vUINT >> 30;
	pml4Index = vUINT >> 39;

	if (!(cRecursivePagingPML4Base[pml4Index] & PAGE_PRESENT)) {
		cRecursivePagingPML4Base[pml4Index] = ((UINT64)InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM)) | (f | PAGE_PRESENT | PAGE_WRITE);
		PagingTLBFlushPage(cRecursivePagingPDPsBase + 512 * pml4Index);
		MemSet(cRecursivePagingPDPsBase + 512 * pml4Index, 0, PAGE_SIZE);
	}

	if (!(cRecursivePagingPDPsBase[pdpIndex] & PAGE_PRESENT)) {
		cRecursivePagingPDPsBase[pdpIndex] = ((UINT64)InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM)) | (f | PAGE_PRESENT | PAGE_WRITE);
		PagingTLBFlushPage(cRecursivePagingPDsBase + 512 * pdpIndex);
		MemSet(cRecursivePagingPDsBase + 512 * pdpIndex, 0, PAGE_SIZE);
	}

	if (!(cRecursivePagingPDsBase[pdIndex] & PAGE_PRESENT)) {
		cRecursivePagingPDsBase[pdIndex] = ((UINT64)InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM)) | (f | PAGE_PRESENT | PAGE_WRITE);
		PagingTLBFlushPage(cRecursivePagingPTsBase + 512 * pdIndex);
		MemSet(cRecursivePagingPTsBase + 512 * pdIndex, 0, PAGE_SIZE);
	}

	if (cRecursivePagingPTsBase[ptIndex] & PAGE_PRESENT) {
		if(gInteruptInitialized)
			PrintT("Warning: page overriden");
	}

	cRecursivePagingPTsBase[ptIndex] = ((UINT64)p) | f;
	PagingTLBFlushPage(v);
}

void SetupCR4() {
	UINT64 CR4 = GetCR4();
	CR4 &= (~4096); /* ensure level 5 paging is disabled */
	SetCR4(CR4);
}

void SetupCR0() {
	UINT64 CR0 = GetCR0();
	CR0 &= (~65536);
	SetCR0(CR0);
}

UINT64 PhysicalAddressFunctionAllocPages(UINT64 irrelevant, UINT64 relativeIrrelevant) {
	return InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM);
}

UINT64 PhysicalAddressFunctionIdentify(UINT64 a, UINT64 b) {
	return a;
}

UINT64 PhysicalAddressFunctionGlobalMemoryMap(UINT64 address, UINT64 relativeAddrss) {
	return PAGE_ALIGN((UINT64)GlobalPhysicalMemoryMap) + relativeAddrss;
}

void InternalPagingAllocatePagingStructures(
	UINT64* PML4, UINT64 PML4Index, UINT64 PDPIndex, 
	UINT64 initialPageTable, UINT64 howManyPageTables, 
	UINT64(*PhysicalAddressFunction)(UINT64, UINT64), 
	UINT16 additionalFlags) 
{
	UINT64 i, j, *PDPTempKernel, *PDTempKernel;
	if ((PML4[PML4Index] & PAGE_PRESENT) == 0) {
		PML4[PML4Index] = InternalAllocatePhysicalPageWithType(MEM_TYPE_KERNEL);
		MemSet(PML4[PML4Index], 0, PAGE_SIZE);
		PML4[PML4Index] |= (PAGE_WRITE | PAGE_PRESENT | additionalFlags);
	}

	PDPTempKernel = PAGE_ALIGN(PML4[PML4Index]);

	if ((PDPTempKernel[PDPIndex] & PAGE_PRESENT) == 0) {
		PDPTempKernel[PDPIndex] = InternalAllocatePhysicalPageWithType(MEM_TYPE_KERNEL);
		MemSet(PDPTempKernel[PDPIndex], 0, PAGE_SIZE);
		PDPTempKernel[PDPIndex] |= (PAGE_WRITE | PAGE_PRESENT | additionalFlags);
	}
	
	PDTempKernel = PAGE_ALIGN(PDPTempKernel[PDPIndex]);

	/* allocate page tables */
	for (i = initialPageTable; i < howManyPageTables + initialPageTable; i++) {
		PDTempKernel[i] = InternalAllocatePhysicalPageWithType(MEM_TYPE_KERNEL);
		for (j = 0; j < 512; j++) {
			((UINT64*)PDTempKernel[i])[j] = PhysicalAddressFunction(ToCanonicalAddress(((i + (512 * PML4Index + PDPIndex) * 512) * 512 + j) << 12), ((i - initialPageTable) * 512 + j) << 12) 
				| (PAGE_PRESENT | PAGE_WRITE | additionalFlags);
		}

		PDTempKernel[i] |= (PAGE_PRESENT | PAGE_WRITE | additionalFlags);
	}
}

void PagingInit() {
	UINT64 i, *PML4, const pageTableKernelReserve = 16;
	const UINT64 virtualGlobalPhysicalMemoryMapSize = ToCanonicalAddress((KERNEL_PML4_ENTRY * 512 * 512 + pageTableKernelReserve) * 512 * 4096);
	SetupCR0();
	SetupCR4();

	PML4 = InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM);
	MemSet(PML4, 0, PAGE_SIZE);

	/* for loader-temporary kernel */
	InternalPagingAllocatePagingStructures(PML4, 0, 0, 0, 32, PhysicalAddressFunctionIdentify, 0); 

	/* for stack */
	InternalPagingAllocatePagingStructures(PML4, 0, 0, 127, 1, PhysicalAddressFunctionIdentify, 0);

	/* for main kernel */
	InternalPagingAllocatePagingStructures(PML4, KERNEL_PML4_ENTRY, 0, 0, pageTableKernelReserve, PhysicalAddressFunctionAllocPages, PAGE_GLOBAL);

	/* for physical memory map */
	InternalPagingAllocatePagingStructures(PML4, KERNEL_PML4_ENTRY, 0, pageTableKernelReserve, 
		GlobalPhysicalMemoryMapSize / (4096 * 512) + 2, PhysicalAddressFunctionGlobalMemoryMap, 0);

	/* for recursive paging */
	PML4[PML4EntryForRecursivePaging] = ((UINT64)PML4) | (PAGE_WRITE | PAGE_PRESENT);

	SetCR3(PML4);
	GlobalPhysicalMemoryMap = virtualGlobalPhysicalMemoryMapSize | (((UINT64)GlobalPhysicalMemoryMap) & 0xFFF);

	for (i = 0; i < FrameBufferSize()/ PAGE_SIZE +1; i++) {
		PagingMapPage(FRAMEBUFFER_DESIRED_LOCATION+i* PAGE_SIZE, ((UINT64)gFramebuffer)+i* PAGE_SIZE, 0x3);
		GlobalPhysicalMemoryMap[((UINT64)gFramebuffer) / 4096 + i] = MEM_TYPE_USED_PERM;
	}

	TextIOInitialize(FRAMEBUFFER_DESIRED_LOCATION, FRAMEBUFFER_DESIRED_LOCATION + FrameBufferSize(), 0, 0, 0);
	TextIOSetColorInformation(0xFFFFFFFF, 0x00000000, 0);
}
