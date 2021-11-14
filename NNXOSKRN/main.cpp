/*
	All of NNXOSLDR .obj files are included in the linking process of this project
	This is done to minimalize the ammount of code rewritten for the kernel,
	that had already been written for the loader (e.g. FAT code, AML, memory managemenet).

	This makes it possible to link to all loader functions without any stub function or
	pointer passing. All changes to the loader files will immediately, whithout any changes
	to the kernel, be passed to the kernel.

	It is also useful, because the loader resides on address 0x10000 in memory, and
	this kernel is supposed to be on 3 or 2 GiB address boundary (this has some advantages)

	The disadvantage of this solution is the disk space needed to contain the operating system,
	as now there are two copies of each loader function on the disk, but since there are no
	plans of expanding the loader much more, this can do for now.
*/

#include <HAL/ACPI/AML.h>
#include <video/SimpleTextIo.h>
#include <nnxcfg.h>
#include <memory/paging.h>
#include <memory/physical_allocator.h>
#include <HAL/IDT.h>
#include <HAL/APIC/APIC.h>
#include <device/fs/vfs.h>
#include <HAL/MP/MP.h>
#include "bugcheck.h"
#include "HAL/X64/cpu.h"
#include "HAL/X64/pcr.h"
#include "HAL/X64/sheduler.h"
#include <HAL/PIT.h>

int basicallyATest = 0;

extern "C"
{
	VOID DrawMap();
	UINT32* gFramebuffer;
	UINT32* gFramebufferEnd;
	UINT32 gPixelsPerScanline;
	UINT32 gWidth;
	UINT32 gHeight;
	extern UINT32 gMinY;
	extern UINT32 gMinX;
	extern UINT32 gMaxY;
	extern UINT32 gMaxX;
	extern UINT8 Initialized;
	extern VOID(*gExceptionHandlerPtr)(UINT64 n, UINT64 errcode, UINT64 errcode2, UINT64 rip);

	extern UINT8* GlobalPhysicalMemoryMap;
	extern UINT64 GlobalPhysicalMemoryMapSize;
	extern UINT64 MemorySize;


	/*
		Wrapper between ExceptionHandler and KeBugCheck
	*/
	VOID KeExceptionHandler(UINT64 n, UINT64 errcode, UINT64 errcode2, UINT64 rip)
	{
		KeBugCheckEx(BC_KMODE_EXCEPTION_NOT_HANDLED, n, rip, errcode, errcode2);
	}

	UINT64 KeEntry(ACPI_RDSP* pRdsp)
	{
		PKIDTENTRY64 idt;
		DisableInterrupts();
		PitUniprocessorInitialize();
		//while (1);

		gExceptionHandlerPtr = KeExceptionHandler;

		TextIoInitialize(gFramebuffer, gFramebufferEnd, gWidth, gHeight, gPixelsPerScanline);
		TextIoClear();

		/* TODO: free temp-kernel physical memory here */
		DrawMap();

		NNXAllocatorInitialize();
		for (int i = 0; i < 64; i++)
		{
			NNXAllocatorAppend(PagingAllocatePage(), 4096);
		}

		VfsInit();
		PciScan();

		UINT8 status;
		ACPI_FADT* facp = (ACPI_FADT*) AcpiGetTable(pRdsp, "FACP");

		if (!facp)
		{
			status = AcpiLastError();
			ACPI_ERROR(status);
			KeBugCheck(BC_PHASE1_INITIALIZATION_FAILED);
		}

		ACPI_MADT* MADT = (ACPI_MADT*) AcpiGetTable(pRdsp, "APIC");
		PrintT("Found MADT on %x\n", MADT);

		ApicInit(MADT);
		HalpSetupPcrForCurrentCpu(ApicGetCurrentLapicId());

		if (status = PspDebugTest())
			KeBugCheckEx(BC_PHASE1_INITIALIZATION_FAILED, status, 0, 0, 0);

		MpInitialize();

		while (1);
		KeBugCheck(BC_PHASE1_INITIALIZATION_FAILED);
	}
}