#include "hdd.h"
#include "../fs/gpt.h"
#include "../fs/mbr.h"
#include "HAL/PCI/PCI.h"
#include "HAL/PCI/PCIIDE.h"
#include "../fs/fat.h"
#include "../fs/fat32.h"
#include "../fs/vfs.h"
#include "../../memory/nnxalloc.h"

BOOL RegisterPartition(UINT64 number)
{
	VIRTUAL_FILE_SYSTEM* filesystem = VfsGetPointerToVfs(number);

	BPB _bpb, *bpb = &_bpb;
	VfsReadSector(filesystem, 0, bpb);

	BYTE *FatTypeInfo = 0, *VolumeLabel = 0;
	UINT32 serialNumber = 0;
	bool HasNameOrID = false;
	if (bpb->SectorTotSize16 == 0 && bpb->SectorTotSize32 == 0)
{
		PrintT("No FAT\n");
		PrintT("Unsupported virtual filesystem of %i\n", number);
		return FALSE;

	}
	else if (bpb->RootEntryCount == 0)
	{
		PrintT("FAT32 disk detected\n");
		BPB_EXT_FAT32* extBPB = &(bpb->_);
		FatTypeInfo = extBPB->FatTypeInfo;
		if (extBPB->HasNameOrID == 0x29)
{
			serialNumber = extBPB->VolumeID;
			VolumeLabel = extBPB->VolumeLabel;
		}

	}
	else
{
		PrintT("FAT12/16 disk detected\n");
		BPB_EXT_FAT1X* extBPB = &(bpb->_);
		FatTypeInfo = extBPB->FatTypeInfo;
		if (extBPB->HasNameOrID == 0x29);
		{
			serialNumber = extBPB->VolumeID;
			VolumeLabel = extBPB->VolumeLabel;
		}
	}

	if (FatTypeInfo)
{
		filesystem->AllocationUnitSize = bpb->SectorsPerCluster * bpb->BytesPerSector;
		BYTE nameCopy[12] = { 0 };

		for (int n = 0; n < 8; n++)
{
			nameCopy[n] = FatTypeInfo[n];
		}
		PrintT("(%s)\n", nameCopy);

		if (VolumeLabel)
{
			for (int n = 0; n < 11; n++)
{
				nameCopy[n] = VolumeLabel[n];
			}
			PrintT("%s %x\n", nameCopy, serialNumber);
		}
		else
{
			PrintT("No volume name/ID info.\n");
		}

		UINT32 freeClusters = FatScanFree(filesystem);
		PrintT("%i/%i KiB free\n", freeClusters * bpb->BytesPerSector * bpb->SectorsPerCluster / 1024, FatCalculateFatClusterCount(bpb) * bpb->BytesPerSector * bpb->SectorsPerCluster / 1024);
		bool pass = NNXFatAutomaticTest(filesystem);
		if (!pass)
			PrintT("Test failed.\n");
	}
	else
{
		PrintT("\n");
	}

	return TRUE;
}

void DiskCheck()
{

	UINT8 diskReadBuffer[4096];
	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS * 4; i++)
{
		if (!drives[i].Reserved || drives[i].type)
			continue;
		MBR mbr;
		PciIdeDiskIo(drives + i, 0, 0, 1, &diskReadBuffer);
		mbr = *((MBR*)diskReadBuffer);
		if (mbr.mbrtable.magicNumber == MBR_SIGNATURE)
{
			GPT gpt;
			PciIdeDiskIo(drives + i, 0, 1, 1, &gpt);
			
			UINT32 number = -1;

			if (gpt.header.signature == GPT_SIGNATURE)
{
				//TODO: GPT Disks
				PrintT("An GPT disk. (todo)\n");
				UINT32 bytesPerEntry = gpt.header.bytesPerEntry;
				UINT32 numberOfEntries = gpt.header.numberOfPartitionTableEntries;
				UINT32 startOfArray = gpt.header.lbaOfPartitionTable;

				UINT8 buffer[1024];
				UINT32 entryNumber = 0;

				UINT32 sectorCountForPartitionArray = (numberOfEntries * bytesPerEntry - 1) / 512 + 1;
				for (UINT32 currentSector = 0; currentSector < sectorCountForPartitionArray; currentSector++)
{
					PciIdeDiskIo(drives + i, 0, startOfArray + currentSector, 2, buffer);

					UINT32 entryStartInSector = entryNumber * bytesPerEntry % 512;
					for (UINT32 currentEntryPos = entryStartInSector; currentEntryPos < 512; currentEntryPos += bytesPerEntry)
{
						GPTPartitionEntry* entry = (buffer + currentEntryPos);
						if (!GPTCompareGUID(entry->typeGUID, GPT_EMPTY_TYPE))
{
							number = VfsAddPartition(drives + i, entry->lbaPartitionStart, entry->lbaPartitionEnd - entry->lbaPartitionStart - 1, FatVfsInterfaceGetFunctionSet());
							FatInitVfs(VfsGetPointerToVfs(number));
						}
						entryNumber++;
					}
				}
			}
			else
{
				PrintT("An MBR disk.\n");
				for (int partitionNumber = 0; partitionNumber < 4; partitionNumber++)
{
					MBRPartitionTableEntry entry = mbr.mbrtable.tableEntries[partitionNumber];
					
					if (entry.partitionSizeInSectors == 0)
						continue;

					number = VfsAddPartition(drives + i, entry.partitionStartLBA28, entry.partitionSizeInSectors, FatVfsInterfaceGetFunctionSet());
					FatInitVfs(VfsGetPointerToVfs(number));
				}
			}

			RegisterPartition(number);

		}
		else
{
			PrintT("Drive %i not formatted, signature 0x%X\n", i, mbr.mbrtable.magicNumber);
		}
	}
}