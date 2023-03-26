#define NNX_ALLOC_DEBUG 1
#include <nnxalloc.h>
#include <HALX64/include/MP.h>
#include <HALX64/include/APIC.h>
#include <HALX64/include/PIT.h>
#include <paging.h>
#include <vfs.h>
#include <pcr.h>
#include <bugcheck.h>
#include <scheduler.h>

extern "C" {

	extern UINT KeNumberOfProcessors;
	PVOID* ApStackPointerArray;

	PVOID MpPopulateApStartupCode()
	{
		VFS_FILE* apCodeFile;
		VFS* systemPartition;
		PVOID code = (PVOID) 0x0000;
		PagingMapPage(0x0000, 0x0000, PAGE_WRITE | PAGE_PRESENT);
		_AP_DATA* data = (_AP_DATA*) (((UINT64) code) + 0x800);
		systemPartition = VfsGetSystemVfs();

		apCodeFile = systemPartition->Functions.OpenFile(
			systemPartition, 
			(char*)"EFI\\BOOT\\APSTART.BIN");

		if (!apCodeFile)
		{
			PrintT("Error loading file\n");
			while (1);
		}
		else
		{
			systemPartition->Functions.ReadFile(apCodeFile, PAGE_SIZE, code);
			systemPartition->Functions.CloseFile(apCodeFile);
		}

		data->ApCR3 = PagingGetAddressSpace();
		data->ApStackPointerArray = ApStackPointerArray;
		data->ApProcessorInit = ApProcessorInit;
		_sgdt(&data->ApGdtr);
		__sidt(&data->ApIdtr);

		return code;
	}

	UINT32 debugColors[] = {
		0xFF000000,
		0xFF0000AA,
		0xFF00AA00,
		0xFF00AAAA,
		0xFFAA0000,
		0xFFAA00AA,
		0xFFAA5500,
		0xFFAAAAAA,
		0xFF555555,
		0xFF5555FF,
		0xFF55FF55,
		0xFF55FFFF,
		0xFFFF5555,
		0xFFFF55FF,
		0xFFFFFF55,
		0xFFFFFFFF
	};

	UINT32 color = 0;

	VOID MpInitialize()
	{
		UINT64 i, j;
		UINT8 currentLapicId;
		PVOID apData, apCode;
		NTSTATUS status;

		currentLapicId = ApicGetCurrentLapicId();
		KeNumberOfProcessors = ApicNumberOfCoresDetected;
		
		if (ApicNumberOfCoresDetected != 1)
		{
			ApStackPointerArray = (PVOID*)ExAllocatePool(
				NonPagedPool,
				ApicNumberOfCoresDetected * sizeof(*ApStackPointerArray));

			apCode = MpPopulateApStartupCode();
			apData = (VOID*)(((UINT64)apCode) + 0x800);

			ApicClearError();
			for (i = 0; i < ApicNumberOfCoresDetected; i++)
			{
				if (ApicLocalApicIDs[i] == currentLapicId)
					continue;

				ApStackPointerArray[i] =
					(PVOID)((ULONG_PTR)PagingAllocatePageFromRange(
						PAGING_KERNEL_SPACE,
						PAGING_KERNEL_SPACE_END) + PAGE_SIZE);

				ApicInitIpi(ApicLocalApicIDs[i], 0x00);
				PitUniprocessorPollSleepMs(10);

				for (j = 0; j < 2; j++)
				{
					ApicClearError();

					if ((UINT64)apCode > UINT16_MAX)
						KeBugCheck(HAL_INITIALIZATION_FAILED);

					ApicStartupIpi(ApicLocalApicIDs[i], 0, (UINT16)(UINT64)apCode);
					PitUniprocessorPollSleepUs(200);
				}
			}
		}
		status = PspInitializeCore(currentLapicId);
		PrintT("PspInitializeCore NTSTATUS: %X\n", status);
		if (status)
			KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, status, 0, 0, 0);
	}

	__declspec(align(64)) UINT64 DrawLock = 0;

	VOID ApProcessorInit(UINT8 lapicId)
	{
		NTSTATUS status;

		HalpSetDummyPcr();

		HalpSetupPcrForCurrentCpu(lapicId);
		ApicLocalApicInitializeCore();
		status = PspInitializeCore(lapicId);

		if (status)
			KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, status, 0, 0, 0);

		/* The system shouldn't get here anyway. */
		KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, 0, 0, 0, 0);
	}
}