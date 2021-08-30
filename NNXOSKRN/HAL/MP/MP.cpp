#include "MP.h"
#include "../APIC/APIC.h"
#include "../../../NNXOSLDR/HAL/PIT.h"
#include "../../../NNXOSLDR/memory/paging.h"
#include "../../../NNXOSLDR/device/fs/vfs.h"

extern "C" {

	PVOID* ApStackPointerArray;

	PVOID MpPopulateApStartupCode() {
		VFSFile* apCodeFile;
		VFS* systemPartition;
		PVOID code = (PVOID)0x0000;
		ApData* data = (ApData*)(((UINT64)code) + 0x800);
		systemPartition = VFSGetSystemVFS();

		apCodeFile = systemPartition->functions.OpenFile(systemPartition, (char*)"EFI\\BOOT\\APSTART.BIN");
		if (!apCodeFile) {
			PrintT("Error loading file\n");
			while (1);
		}
		else {
			systemPartition->functions.ReadFile(apCodeFile, 0x1000, code);
			systemPartition->functions.CloseFile(apCodeFile);
		}

		data->ApCR3 = GetCR3();
		data->ApStackPointerArray = ApStackPointerArray;
		data->ApProcessorInit = ApProcessorInit;
		StoreGDT(&data->ApGdtr);
		StoreIDT(&data->ApIdtr);

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

	VOID MpInitialize() {
		UINT64 i, j;
		UINT8 currentLapicId;
		PVOID apData, apCode;

		ApStackPointerArray = (PVOID*)NNXAllocatorAllocArray(ApicNumberOfCoresDetected, sizeof(*ApStackPointerArray));

		apCode = MpPopulateApStartupCode();
		apData = (VOID*)(((UINT64)apCode) + 0x800);

		currentLapicId = ApicGetCurrentLapicId();

		ApicClearError();
		for (i = 0; i < ApicNumberOfCoresDetected; i++) {
			if (ApicLocalApicIDs[i] == currentLapicId)
				continue;

			ApStackPointerArray[i] = (PVOID)NNXAllocatorAlloc(AP_INITIAL_STACK_SIZE);
			ApicInitIpi(ApicLocalApicIDs[i], 0x00);
			PitUniprocessorPollSleepMs(10);

			for (j = 0; j < 2; j++) {
				PrintT("Sending SIPI%i for %x (%i)\n", j, ApicLocalApicIDs[i], i);
				ApicClearError();
				ApicStartupIpi(ApicLocalApicIDs[i], 0, (UINT16)apCode);
				PitUniprocessorPollSleepUs(200);
			}
		}

		PrintT("%s done\n", __FUNCTION__);

		while (true) {
			color++;
			color--;
		}
	}

	VOID ApProcessorInit(UINT8 lapicId) {
		while (true) {
			PitUniprocessorPollSleepMs(998);

			for (int y = 0; y < gHeight / 2; y++) {
				for (int x = 0; x < gWidth / 2; x++) {
					gFramebuffer[y * gPixelsPerScanline + x] = debugColors[color];
				}
			}

			color = (color + 1) % (sizeof(debugColors) / sizeof(*debugColors));
		}
	}
}