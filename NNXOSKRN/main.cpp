#include <HAL/ACPI/AML.h>
#include <SimpleTextIo.h>
#include <nnxcfg.h>
#include <HAL/paging.h>
#include <HAL/physical_allocator.h>
#include <device/fs/vfs.h>
#include <HAL/x64/MP.h>
#include <HAL/X64/APIC.h>
#include <bugcheck.h>
#include <HAL/cpu.h>
#include <HAL/pcr.h>
#include <scheduler.h>
#include <HAL/X64/PIT.h>
#include <nnxlog.h>

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

	ULONG_PTR gRdspPhysical;

	ULONG_PTR KeMaximumIncrement = 15 * 10000;

	UINT KeNumberOfProcessors = 1;

	/*
		Wrapper between ExceptionHandler and KeBugCheck
	*/
	VOID KeExceptionHandler(UINT64 n, UINT64 errcode, UINT64 errcode2, UINT64 rip)
	{
		KeBugCheckEx(KMODE_EXCEPTION_NOT_HANDLED, n, rip, errcode, errcode2);
	}

	__declspec(dllexport) UINT64 KeEntry()
	{
		DisableInterrupts();
		PitUniprocessorInitialize();

		gExceptionHandlerPtr = KeExceptionHandler;

		TextIoInitialize(gFramebuffer, gFramebufferEnd, gWidth, gHeight, gPixelsPerScanline);
		TextIoClear();

		DrawMap();

		NNXAllocatorInitialize();
		for (int i = 0; i < 64; i++)
		{
			NNXAllocatorAppend((PVOID)PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END), 4096);
		}

		VfsInit();
		PciScan();

		ACPI_RDSP* pRdsp = (ACPI_RDSP*) PagingMapStrcutureToVirtual(gRdspPhysical, sizeof(ACPI_RDSP), PAGE_PRESENT | PAGE_WRITE);

		UINT8 status;
		ACPI_FADT* facp = (ACPI_FADT*) AcpiGetTable(pRdsp, "FACP");

		if (!facp)
		{
			status = AcpiLastError();
			AcpiError(status);
			KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, __LINE__, status, 0, 0);
		}

		ACPI_MADT* madt = (ACPI_MADT*) AcpiGetTable(pRdsp, "APIC");
		PrintT("Found MADT on %x\n", madt);

		ApicInit(madt);
		HalpSetupPcrForCurrentCpu(ApicGetCurrentLapicId());

		MpInitialize();

		KeBugCheck(PHASE1_INITIALIZATION_FAILED);
	}
}