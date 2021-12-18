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

	ULONG_PTR gRdspPhysical;

	__declspec(dllexport) UINT64 KeEntry()
	{
		PKIDTENTRY64 idt;

		DisableInterrupts();
		PitUniprocessorInitialize();

		gExceptionHandlerPtr = KeExceptionHandler;

		TextIoInitialize(gFramebuffer, gFramebufferEnd, gWidth, gHeight, gPixelsPerScanline);
		TextIoClear();

		/* TODO: free temp-kernel physical memory here */
		DrawMap();

		NNXAllocatorInitialize();
		for (int i = 0; i < 64; i++)
		{
			NNXAllocatorAppend(PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END), 4096);
		}

		VfsInit();
		PciScan();

		PrintT("Prealloc %X\n", gRdspPhysical);
		ACPI_RDSP* pRdsp = (ACPI_RDSP*) PagingMapStrcutureToVirtual(gRdspPhysical, sizeof(ACPI_RDSP), PAGE_PRESENT | PAGE_WRITE);

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

		MpInitialize();

		while (1);
		KeBugCheck(BC_PHASE1_INITIALIZATION_FAILED);
	}
}