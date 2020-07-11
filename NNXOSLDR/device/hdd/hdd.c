#include "hdd.h"
#include "../fs/gpt.h"
#include "../fs/mbr.h"
#include "HAL/PCI/PCI.h"
#include "HAL/PCI/PCIIDE.h"
#include "../fs/fat.h"
#include "../fs/fat32.h"
#include "../fs/vfs.h"

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
				//TODO
				PrintT("An GPT disk. (todo)\n");
			}
			else {
				PrintT("An MBR disk, GPT signature: 0x%X, should be (in order to be GPT): 0x%X\n", gpt.header.signature, GPT_SIGNATURE);
				for (int partitionNumber = 0; partitionNumber < 4; partitionNumber++) {
					
					UINT8 bpbSource[512];
					MBRPartitionTableEntry entry = mbr.mbrtable.tableEntries[partitionNumber];
					
					if (entry.partitionSizeInSectors == 0)
						continue;

					int number = VFSAddPartition(drives + i, entry.partitionStartLBA28, entry.partitionSizeInSectors);
					VirtualFileSystem* filesystem = VFSGetPointerToVFS(number);
					BPB* bpb = (BPB*)bpbSource;

					VFSReadSector(filesystem, 0, bpb);

					BYTE *fatTypeInfo = 0, *volumeLabel = 0;
					UINT32 serialNumber = 0;
					bool hasNameOrID = false;
					if (bpb->sectorTotSize16 == 0 && bpb->sectorTotSize32 == 0) {
						PrintT("No FAT\n");
						PrintT("Unsupported filesystem of of %i, partition %i.\n",i, partitionNumber);
						while (1);

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
						if(extBPB->hasNameOrID == 0x29);
						{
							serialNumber = extBPB->volumeID;
							volumeLabel = extBPB->volumeLabel;
						}
					}

					if(fatTypeInfo){
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
						FATDebugDirRoot(filesystem);
					}
					else {
						PrintT("\n");
					}
				}
			}

		}
		else {
			PrintT("Drive %i not formatted, signature 0x%X\n", i, mbr.mbrtable.magicNumber);
			while (1);
		}
	}
}