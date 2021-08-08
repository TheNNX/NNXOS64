/* All of NNXOSLDR .obj files are included in the linking process of this project 
   This is done to minimalize the ammount of code rewritten for the kernel,
   that had already been written for the loader (e.g. FAT code, AML, memory managemenet).
   
   This makes it possible to link to all loader functions without any stub function or
   pointer passing. All changes to the loader files will immediately, whithout any changes
   to the kernel, be passed to the kernel.
   
   It is also useful, because the loader resides on address 0x10000 in memory, and 
   this kernel is supposed to be on 3 or 2 GiB address boundary (this has some advantages)
   
   The disadvantage of this solution is the disk space needed to contain the operating system,
   as now there are two copies of each loader function on the disk, but since there are no
   plans of expanding the loader much more, this can do for now. */

#include "HAL/ACPI/AML.h"
#include "../NNXOSLDR/video/SimpleTextIO.h"
#include "../NNXOSLDR/nnxoskldr.h"
#include "../NNXOSLDR/nnxcfg.h"
#include "../NNXOSLDR/memory/paging.h"
#include "../NNXOSLDR/memory/physical_allocator.h"
#include "../NNXOSLDR/HAL/IDT.h"

int basicallyATest = 0;

extern "C" {
	void DrawMap();
	UINT32* gFramebuffer;
	UINT32* gFramebufferEnd;
	UINT32 gPixelsPerScanline;
	UINT32 gWidth;
	UINT32 gHeight;
	extern UINT32 gMinY;
	extern UINT32 gMinX;
	extern UINT32 gMaxY;
	extern UINT32 gMaxX;
	extern UINT8 initialized;
}

extern "C" UINT64 KeEntry(KLdrKernelInitializationData* data) {
	gFramebuffer = data->Framebuffer;
	gFramebufferEnd = data->FramebufferEnd;
	gWidth = data->FramebufferWidth;
	gHeight = data->FramebufferHeight;
	gPixelsPerScanline = data->FramebufferPixelsPerScanline;

	GlobalPhysicalMemoryMap = data->PhysicalMemoryMap;
	GlobalPhysicalMemoryMapSize = data->PhysicalMemoryMapSize;
	for (int i = 0; i < GlobalPhysicalMemoryMapSize; i++) {
		if (GlobalPhysicalMemoryMap[i] == MEM_TYPE_USED)
			GlobalPhysicalMemoryMap[i] = MEM_TYPE_FREE; // deallocate anything allocated by the previous stage
		if (GlobalPhysicalMemoryMap[i] == MEM_TYPE_KERNEL)
			GlobalPhysicalMemoryMap[i] = MEM_TYPE_OLD_KERNEL; // mark critical, but temporary, structures for deletion after setting up the new ones
	}

	TextIOInitialize(gFramebuffer, gFramebufferEnd, gWidth, gHeight, gPixelsPerScanline);
	TextIOClear();

	DrawMap();

	PagingInit();
	PagingMapPage(0x1000, 0x2000, 0x3);
	PagingAllocatePage();

	while (1);

	UINT8 status;
	ACPI_FACP* facp = GetFADT(data->rdsp);
	if (!facp) {
		status = ACPI_LastError();
		ACPI_ERROR(status);
		while (1);
	}

	if (data->rdsp->Revision == 0)
		status = ACPI_ParseDSDT((ACPI_DSDT*)facp->DSDT);
	else
		status = ACPI_ParseDSDT((ACPI_DSDT*)facp->X_DSDT);

	VOID* MADT = GetACPITable(data->rdsp, "APIC");
	PrintT("Found MADT on %x\n", MADT);

	while(1);
}
