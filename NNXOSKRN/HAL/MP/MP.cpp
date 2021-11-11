#include "MP.h"
#include "../APIC/APIC.h"
#include "../../../NNXOSLDR/HAL/PIT.h"
#include "../../../NNXOSLDR/memory/paging.h"
#include "../../../NNXOSLDR/device/fs/vfs.h"
#include "../X64/pcr.h"

extern "C" {

	PVOID* ApStackPointerArray;

	PVOID MpPopulateApStartupCode()
	{
		VFS_FILE* apCodeFile;
		VFS* systemPartition;
		PVOID code = (PVOID) 0x0000;
		ApData* data = (ApData*) (((UINT64) code) + 0x800);
		systemPartition = VfsGetSystemVfs();

		apCodeFile = systemPartition->Functions.OpenFile(systemPartition, (char*)"EFI\\BOOT\\APSTART.BIN");
		if (!apCodeFile)
		{
			PrintT("Error loading file\n");
			while (1);
		}
		else
		{
			systemPartition->Functions.ReadFile(apCodeFile, 0x1000, code);
			systemPartition->Functions.CloseFile(apCodeFile);
		}

		data->ApCR3 = GetCR3();
		data->ApStackPointerArray = ApStackPointerArray;
		data->ApProcessorInit = ApProcessorInit;
		HalpStoreGdt(&data->ApGdtr);
		HalpStoreIdt(&data->ApIdtr);

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

		ApStackPointerArray = (PVOID*) NNXAllocatorAllocArray(ApicNumberOfCoresDetected, sizeof(*ApStackPointerArray));

		apCode = MpPopulateApStartupCode();
		apData = (VOID*) (((UINT64) apCode) + 0x800);

		currentLapicId = ApicGetCurrentLapicId();

		ApicClearError();
		for (i = 0; i < ApicNumberOfCoresDetected; i++)
		{
			if (ApicLocalApicIDs[i] == currentLapicId)
				continue;

			ApStackPointerArray[i] = (PVOID) ((size_t) NNXAllocatorAlloc(AP_INITIAL_STACK_SIZE) + AP_INITIAL_STACK_SIZE);
			ApicInitIpi(ApicLocalApicIDs[i], 0x00);
			PitUniprocessorPollSleepMs(10);

			for (j = 0; j < 2; j++)
			{
				PrintT("Sending SIPI%i for %x (%i)\n", j, ApicLocalApicIDs[i], i);
				ApicClearError();
				ApicStartupIpi(ApicLocalApicIDs[i], 0, (UINT16) apCode);
				PitUniprocessorPollSleepUs(200);
			}
		}
	}

	VOID HalAcquireLockRaw(UINT64* lock);
	VOID HalReleaseLockRaw(UINT64* lock);

	__declspec(align(64)) UINT64 DrawLock = 0;

	VOID ApProcessorInit(UINT8 lapicId)
	{
		HalpSetupPcrForCurrentCpu(lapicId);

		while (true)
		{
			HalAcquireLockRaw(&DrawLock);
			PitUniprocessorPollSleepMs(200);

			for (int y = gHeight - (lapicId + 1) * 40; y < gHeight - lapicId * 40; y++)
			{
				for (int x = 0; x < gWidth / 2; x++)
				{
					gFramebuffer[y * gPixelsPerScanline + x] = debugColors[color];
				}
			}

			color = (color + 1) % (sizeof(debugColors) / sizeof(*debugColors));
			HalReleaseLockRaw(&DrawLock);
		}
	}
}