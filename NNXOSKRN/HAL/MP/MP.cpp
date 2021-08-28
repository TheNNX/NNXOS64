#include "MP.h"
#include "../APIC/APIC.h"
#include "../../../NNXOSLDR/HAL/PIT.h"
#include "../../../NNXOSLDR/memory/paging.h"
#include "../../../NNXOSLDR/device/fs/vfs.h"

extern "C" {

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
		// TODO: rest of ApData

		return code;
	}

	VOID MpInitialize() {
		UINT64 i, j;
		UINT8 currentLapicId;
		PVOID apData, apCode;

		apCode = MpPopulateApStartupCode();
		apData = (VOID*)(((UINT64)apCode) + 0x800);

		currentLapicId = ApicGetCurrentLapicId();
		
		ApicClearError();
		for (i = 0; i < ApicNumberOfCoresDetected; i++) {
			if (ApicLocalApicIDs[i] == currentLapicId)
				continue;

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
	}
}