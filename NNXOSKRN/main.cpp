#include <HAL/ACPI/ACPI.h>
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
#include <nnxver.h>
#include <ob/object.h>
#include <pool.h>
#include <device/Keyboard.h>
#include <HAL/X64/cmos.h>
#include <HAL/rtc.h>
#include <HAL/syscall.h>

extern "C"
{
	VOID UpdateScreen();
	VOID DrawMap();
	volatile UINT32* gFramebuffer;
	volatile UINT32* gFramebufferEnd;
	UINT32 gPixelsPerScanline;
	UINT32 gWidth;
	UINT32 gHeight;
	extern UINT32 gMinY;
	extern UINT32 gMinX;
	extern UINT32 gMaxY;
	extern UINT32 gMaxX;
	extern UINT8 Initialized;
	extern VOID(*gExceptionHandlerPtr)(UINT64 n, UINT64 errcode, UINT64 errcode2, UINT64 rip);
	extern UINT64 MemorySize;

	ULONG_PTR gRdspPhysical;
	ULONG_PTR KeMaximumIncrement = 15 * 10000;
	UINT KeNumberOfProcessors = 1;

	VOID 
	KeExceptionHandler(
		UINT64 n, 
		UINT64 errcode, 
		UINT64 errcode2, 
		UINT64 rip)
	{
		KeBugCheckEx(
			KMODE_EXCEPTION_NOT_HANDLED, 
			n,
			rip, 
			errcode, 
			errcode2);
	}

	__declspec(dllimport) const char* HalTest();

	__declspec(dllexport) 
	UINT64 
	KeEntry()
	{
        NTSTATUS Status;
		ULONG64 CurrentTime;

		HalpInitDummyPcr();
		HalpSetDummyPcr();

		DisableInterrupts();
		PitUniprocessorInitialize();

		gExceptionHandlerPtr = KeExceptionHandler;

		TextIoInitialize(
			gFramebuffer, 
			gFramebufferEnd, 
			gWidth,
			gHeight,
			gPixelsPerScanline);

		TextIoClear();
		MmReinitPhysAllocator(PfnEntries, NumberOfPfnEntries);

		Status = ExInitializePool(
			(PVOID)PagingAllocatePageBlockFromRange(
				32, 
				PAGING_KERNEL_SPACE,
				PAGING_KERNEL_SPACE_END),
			PAGE_SIZE * 32,
			(PVOID)PagingAllocatePageBlockFromRange(
				64, 
				PAGING_KERNEL_SPACE,
				PAGING_KERNEL_SPACE_END),
			PAGE_SIZE * 64);
		if (!NT_SUCCESS(Status))
		{
			KeBugCheck(HAL_INITIALIZATION_FAILED);
		}

		Status = ExPoolSelfCheck();
		if (!NT_SUCCESS(Status))
		{
			KeBugCheck(HAL_INITIALIZATION_FAILED);
		}

		DrawMap();
		VfsInit();

		PciScan();

		ACPI_RDSP* pRdsp = (ACPI_RDSP*) 
			PagingMapStrcutureToVirtual(
				gRdspPhysical, 
				sizeof(ACPI_RDSP), 
				PAGE_PRESENT | PAGE_WRITE | PAGE_NO_CACHE);

		UINT8 bAcpiError;
		ACPI_FADT* pFacp = (ACPI_FADT*) AcpiGetTable(pRdsp, "FACP");

		if (!pFacp)
		{
            bAcpiError = AcpiLastError();
			AcpiError(bAcpiError);

			KeBugCheckEx(
				PHASE1_INITIALIZATION_FAILED, 
				__LINE__, 
				bAcpiError, 
				0,
				0);
		}

		ACPI_MADT* pMadt = (ACPI_MADT*) AcpiGetTable(pRdsp, "APIC");

		if (!NT_SUCCESS(KeInitializeDispatcher()))
		{
			KeBugCheckEx(
				PHASE1_INITIALIZATION_FAILED, 
				__LINE__, 
				0, 
				0, 
				0);
		}

		ApicInit(pMadt);

		SetupSystemCallHandler(SystemCallHandler);
		HalpSetupPcrForCurrentCpu(ApicGetCurrentLapicId());
		KeyboardInitialize();

		PrintT(
			"%s %i.%i.%i.%i, compiled %s %s\n",
			NNX_OSNAME, 
			NNX_MAJOR, 
			NNX_MINOR, 
			NNX_PATCH, 
			NNX_BUILD,
			__DATE__,
			__TIME__);

        Status = ObInit();
		if (!NT_SUCCESS(Status))
		{
			KeBugCheckEx(
				PHASE1_INITIALIZATION_FAILED, 
				__LINE__, 
				Status, 
				0,
				0);
		}

		PagingInitializePageFile(
			16 * PAGE_SIZE,
			"PAGEFILE.SYS", 
			VfsGetSystemVfs());

		CmosInitialize();
		HalRtcInitialize(pFacp->CenturyRegister);
		
		KeQuerySystemTime(&CurrentTime);
		PrintT("Current date and time (%i): ", CurrentTime);
		HalpPrintCurrentDate();
		PrintT(" ");
		HalpPrintCurrentTime();
		PrintT("\n");

		MpInitialize();

		KeBugCheck(PHASE1_INITIALIZATION_FAILED);
	}
}