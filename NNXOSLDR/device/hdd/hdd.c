#include "hdd.h"
#include "../fs/gpt.h"
#include "../fs/mbr.h"
#include "HAL/PCI/PCI.h"
#include "HAL/PCI/PCIIDE.h"

void diskCheck() {
	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS * 4; i++) {
		if (drives[i].reserved) {
			PrintT("   %x   -  %s Drive [%iMiB]\n", drives[i].signature, (const char*[]) { "ATA", "ATAPI" }[drives[i].type], drives[i].size / 1024 / 1024);
		}
	}

	UINT8 diskReadBuffer[4096];
	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS * 4; i++) {
		if (!drives[i].reserved)
			continue;
		PrintT("\nTesting %s drive %i of size = %iMiB\n", (const char*[]) { "ATA", "ATAPI" }[drives[i].type], i, drives[i].size / 1024 / 1024);
		MBR mbr;
		PCI_IDE_DiskIO(drives + i, 0, 0, 1, &diskReadBuffer);
		mbr = *((MBR*)diskReadBuffer);
		if (mbr.mbrtable.magicNumber == MBR_SIGNATURE) {
			PrintT("Found drive signature: 0x%X\n\nPartitions:\n", MBR_SIGNATURE);
			GPT gpt;
			PCI_IDE_DiskIO(drives + i, 0, 1, 1, &gpt);
			if (gpt.header.signature == GPT_SIGNATURE) {
				PrintT("An GPT disk.\n");
			}
			else {
				PrintT("An MBR disk, GPT signature: 0x%X, should be (in order to be GPT): 0x%X\n", gpt.header.signature, GPT_SIGNATURE);
				for (int partitionNumber = 0; partitionNumber < 4; partitionNumber++) {
					MBRPartitionTableEntry entry = mbr.mbrtable.tableEntries[partitionNumber];
					PrintT("  %i: %x %x\n", partitionNumber, entry.partitionType, entry.attributes);


				}
			}

		}
		else {
			PrintT("Drive %i not formatted, signature 0x%X\n", i, mbr.mbrtable.magicNumber);
		}
	}
}