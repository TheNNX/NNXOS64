#include "hdd.h"
#include "../fs/gpt.h"
#include "../fs/mbr.h"
#include "HAL/PCI/PCI.h"
#include "HAL/PCI/PCIIDE.h"
#include "../fs/fat.h"
#include "../fs/fat32.h"
#include "../fs/vfs.h"
#include "../../memory/nnxalloc.h"

BOOL RegisterPartition(UINT64 number) {
	VirtualFileSystem* filesystem = VFSGetPointerToVFS(number);

	BPB _bpb, *bpb = &_bpb;
	VFSReadSector(filesystem, 0, bpb);

	BYTE *fatTypeInfo = 0, *volumeLabel = 0;
	UINT32 serialNumber = 0;
	bool hasNameOrID = false;
	if (bpb->sectorTotSize16 == 0 && bpb->sectorTotSize32 == 0) {
		PrintT("No FAT\n");
		PrintT("Unsupported virtual filesystem of %i\n", number);
		return FALSE;

	}
	else if (bpb->rootEntryCount == 0)
	{
		PrintT("FAT32 ");
		BPB_EXT_FAT32* extBPB = &(bpb->_);
		fatTypeInfo = extBPB->fatTypeInfo;
		if (extBPB->hasNameOrID == 0x29) {
			serialNumber = extBPB->volumeID;
			volumeLabel = extBPB->volumeLabel;
		}

	}
	else {
		PrintT("FAT12/16 ");
		BPB_EXT_FAT1X* extBPB = &(bpb->_);
		fatTypeInfo = extBPB->fatTypeInfo;
		if (extBPB->hasNameOrID == 0x29);
		{
			serialNumber = extBPB->volumeID;
			volumeLabel = extBPB->volumeLabel;
		}
	}

	if (fatTypeInfo) {
		filesystem->allocationUnitSize = bpb->sectorsPerCluster * bpb->bytesPerSector;
		BYTE nameCopy[12] = { 0 };

		for (int n = 0; n < 8; n++) {
			nameCopy[n] = fatTypeInfo[n];
		}
		PrintT("(%s)\n", nameCopy);

		if (volumeLabel) {
			for (int n = 0; n < 11; n++) {
				nameCopy[n] = volumeLabel[n];
			}
			PrintT("%s %x\n", nameCopy, serialNumber);
		}
		else {
			PrintT("No volume name/ID info.\n");
		}

		UINT32 freeClusters = FATScanFree(filesystem);
		PrintT("%i/%i KiB free\n", freeClusters * bpb->bytesPerSector * bpb->sectorsPerCluster / 1024, FATCalculateFATClusterCount(bpb) * bpb->bytesPerSector * bpb->sectorsPerCluster / 1024);
		bool pass = NNX_FATAutomaticTest(filesystem);
		if (!pass)
			PrintT("Test failed.\n");
	}
	else {
		PrintT("\n");
	}

	return TRUE;
}

void DiskCheck() {
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
			
			UINT32 number = -1;

			if (gpt.header.signature == GPT_SIGNATURE) {
				//TODO
				PrintT("An GPT disk. (todo)\n");
				UINT32 bytesPerEntry = gpt.header.bytesPerEntry;
				UINT32 numberOfEntries = gpt.header.numberOfPartitionTableEntries;
				UINT32 startOfArray = gpt.header.lbaOfPartitionTable;

				UINT8 buffer[1024];
				UINT32 entryNumber = 0;

				UINT32 sectorCountForPartitionArray = (numberOfEntries * bytesPerEntry - 1) / 512 + 1;
				for (UINT32 currentSector = 0; currentSector < sectorCountForPartitionArray; currentSector++) {
					PCI_IDE_DiskIO(drives + i, 0, startOfArray + currentSector, 2, buffer);

					UINT32 entryStartInSector = entryNumber * bytesPerEntry % 512;
					for (UINT32 currentEntryPos = entryStartInSector; currentEntryPos < 512; currentEntryPos += bytesPerEntry) {
						GPTPartitionEntry* entry = (buffer + currentEntryPos);
						if (!GPTCompareGUID(entry->typeGUID, GPT_EMPTY_TYPE)) {
							number = VFSAddPartition(drives + i, entry->lbaPartitionStart, entry->lbaPartitionEnd - entry->lbaPartitionStart - 1);
						}
						entryNumber++;
					}
				}
			}
			else {
				PrintT("An MBR disk.\n");
				for (int partitionNumber = 0; partitionNumber < 4; partitionNumber++) {
					MBRPartitionTableEntry entry = mbr.mbrtable.tableEntries[partitionNumber];
					
					if (entry.partitionSizeInSectors == 0)
						continue;

					number = VFSAddPartition(drives + i, entry.partitionStartLBA28, entry.partitionSizeInSectors);
				}
			}

			RegisterPartition(number);

		}
		else {
			PrintT("Drive %i not formatted, signature 0x%X\n", i, mbr.mbrtable.magicNumber);
		}
	}
}