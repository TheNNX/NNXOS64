#include "hdd.h"
#include "../fs/gpt.h"
#include "../fs/mbr.h"
#include "HAL/PCI/PCI.h"
#include "HAL/PCI/PCIIDE.h"
#include "../fs/fat.h"
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

					VFSReadSector(filesystem, bpb);

					for (int bpbi = 0; bpbi < 512; bpbi++) {
						if (0xff & (((char*)bpb)[bpbi]) > ' ' && 0xff & (((char*)bpb)[bpbi]) < 0x7f) {
							PrintT("%c ", 0xff & (((char*)bpb)[bpbi]));
						}
						else {
							PrintT("%x ", 0xff & (((char*)bpb)[bpbi]));
						}
					}

					if (bpb->sectorTotSize16 == 0 && bpb->sectorTotSize32 == 0) {
						PrintT("No FAT\n");
						
					}
					else if (bpb->rootEntryCount == 0)
					{
						//FAT32
						PrintT("FAT32\n");
					}
					else {
						PrintT("FAT");
						BPB_EXT_FAT1X* extBPB = ((UINT64)bpb) + sizeof(BPB);
						BYTE nameCopy[12];
						nameCopy[8] = 0;
						nameCopy[11] = 0;
						for (int n = 0; n < 8; n++) {
							nameCopy[n] = extBPB->fatTypeInfo[n];
						}
						PrintT("(%s)\n", nameCopy);
						if (extBPB->hasNameorID == 0x29) {
							for (int n = 0; n < 11; n++) {
								nameCopy[n] = extBPB->volumeLabel[n];
							}
							PrintT("%s %x\n", nameCopy, extBPB->volumeSerialNumber);
						}
						else {
							PrintT("No volume name/ID info.\n");
						}
					}
				}
			}

		}
		else {
			PrintT("Drive %i not formatted, signature 0x%X\n", i, mbr.mbrtable.magicNumber);
		}
	}
}