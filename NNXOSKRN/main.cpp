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
#include <nnxver.h>
#include <ob/object.h>
#include <pool.h>
#include <device/Keyboard.h>

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
        NTSTATUS status;

        KPCR dummyPcr = { 0 };
        dummyPcr.Irql = DISPATCH_LEVEL;
        dummyPcr.Prcb = NULL;
        dummyPcr.SelfPcr = &dummyPcr;

        /* 
            for all the code that for example has to lock after initialization 
            if not for this dummy PCR, access to the IRQL would page fault 

            the correct one is set before multi processing init
        */
        HalSetPcr(&dummyPcr);

		DisableInterrupts();
		PitUniprocessorInitialize();

		gExceptionHandlerPtr = KeExceptionHandler;

		TextIoInitialize(
			gFramebuffer, 
			gFramebufferEnd, 
			gWidth,
			gHeight,
			gPixelsPerScanline
		);

		TextIoClear();

		DrawMap();

		status = ExInitializePool(
			(PVOID)PagingAllocatePageBlockFromRange(32, PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END),
			PAGE_SIZE * 32,
			(PVOID)PagingAllocatePageBlockFromRange(32, PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END),
			PAGE_SIZE * 32
		);

		if (status)
			KeBugCheck(HAL_INITIALIZATION_FAILED);

		status = ExPoolSelfCheck();
		if (status)
			KeBugCheck(HAL_INITIALIZATION_FAILED);

		NNXAllocatorInitialize();
		for (int i = 0; i < 32; i++)
		{
			NNXAllocatorAppend((PVOID)PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END), 4096);
		}

		VfsInit();
		PciScan();

		ACPI_RDSP* pRdsp = (ACPI_RDSP*) PagingMapStrcutureToVirtual(gRdspPhysical, sizeof(ACPI_RDSP), PAGE_PRESENT | PAGE_WRITE | PAGE_NO_CACHE);

		UINT8 acpiError;
		ACPI_FADT* facp = (ACPI_FADT*) AcpiGetTable(pRdsp, "FACP");

		if (!facp)
		{
            acpiError = AcpiLastError();
			AcpiError(acpiError);
			KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, __LINE__, acpiError, 0, 0);
		}

		ACPI_MADT* madt = (ACPI_MADT*) AcpiGetTable(pRdsp, "APIC");

		ApicInit(madt);

		KeyboardInitialize();
		HalpSetupPcrForCurrentCpu(ApicGetCurrentLapicId());

		PrintT("%s %i.%i.%i.%i, compiled %s %s\n", NNX_OSNAME, NNX_MAJOR, NNX_MINOR, NNX_PATCH, NNX_BUILD, __DATE__, __TIME__);

        status = ObInit();

		PagingInitializePageFile(16 * PAGE_SIZE, "PAGEFILE.SYS", VfsGetSystemVfs());

        if (status)
            KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, __LINE__, status, 0, 0);

		MpInitialize();

		KeBugCheck(PHASE1_INITIALIZATION_FAILED);
	}
}