#include "hdd.h"
#include "../fs/gpt.h"
#include "../fs/mbr.h"
#include <HAL/PCI/PCI.h>
#include <HAL/PCI/PCIIDE.h>
#include "../fs/fat.h"
#include "../fs/fat32.h"
#include "../fs/vfs.h"
#include <nnxalloc.h>

BOOL RegisterPartition(UINT64 Number)
{
    VIRTUAL_FILE_SYSTEM *filesystem;

	BPB _bpb = { 0 }, *bpb = &_bpb;
	BYTE *fatTypeInfo = 0, *volumeLabel = 0;

	UINT32 serialNumber = 0;
	BOOL hasNameOrID = FALSE;

    filesystem = VfsGetPointerToVfs(Number);
	VfsReadSector(filesystem, 0, (PBYTE)bpb);

	if (bpb->SectorTotSize16 == 0 && bpb->SectorTotSize32 == 0)
	{
		PrintT("Not FAT\n");
		PrintT("Unsupported virtual filesystem of %i\n", Number);
		return FALSE;

	}
	else if (bpb->RootEntryCount == 0)
	{
		BPB_EXT_FAT32* extBPB = (BPB_EXT_FAT32*)&(bpb->_);
		
        PrintT("FAT32 disk detected\n");
		
        fatTypeInfo = extBPB->FatTypeInfo;
		
        if (extBPB->HasNameOrID == 0x29)
		{
			serialNumber = extBPB->VolumeID;
			volumeLabel = extBPB->VolumeLabel;
		}

	}
	else
	{
		BPB_EXT_FAT1X* extBPB = (BPB_EXT_FAT1X*)&(bpb->_);

		PrintT("FAT12/16 disk detected\n");

		fatTypeInfo = extBPB->FatTypeInfo;
		
        if (extBPB->HasNameOrID == 0x29);
		{
			serialNumber = extBPB->VolumeID;
			volumeLabel = extBPB->VolumeLabel;
		}
	}

	if (fatTypeInfo)
	{
        BOOL automaticTestResult;
        int n;
        UINT32 freeClusters;
        UCHAR nameCopy[12] = { 0 };
        
        filesystem->AllocationUnitSize = (UINT64)bpb->SectorsPerCluster * (UINT64)bpb->BytesPerSector;

		for (n = 0; n < 8; n++)
		{
			nameCopy[n] = fatTypeInfo[n];
		}
		
        PrintT("FAT disk: (%s)\n", nameCopy);

		if (volumeLabel)
		{
			for (n = 0; n < 11; n++)
			{
				nameCopy[n] = volumeLabel[n];
			}
			PrintT("%s %x\n", nameCopy, serialNumber);
		}
		else
		{
			PrintT("No volume name/ID info.\n");
		}

		freeClusters = FatScanFree(filesystem);
		PrintT("%i/%i KiB free\n", freeClusters * bpb->BytesPerSector * bpb->SectorsPerCluster / 1024, FatCalculateFatClusterCount(bpb) * bpb->BytesPerSector * bpb->SectorsPerCluster / 1024);
		
        automaticTestResult = NNXFatAutomaticTest(filesystem);
		
        if (!automaticTestResult)
			PrintT("FAT Test failed.\n");
	}

	PrintT("\n");

	return TRUE;
}

VOID DiskCheck()
{
	UINT8 diskReadBuffer[4096];
    int i;
    
    PrintT("Checking IDE devices\n");

    for (i = 0; i < MAX_PCI_IDE_CONTROLLERS * 4; i++)
	{
		MBR mbr;
		
        if (!Drives[i].Reserved || Drives[i].type)
			continue;
		
        PciIdeDiskIo(Drives + i, 0, 0, 1, diskReadBuffer);
		mbr = *((MBR*) diskReadBuffer);
		
        if (mbr.mbrtable.MagicNumber == MBR_SIGNATURE)
		{
			GPT gpt = { 0 };
			SIZE_T number = -1;
			
            PciIdeDiskIo(Drives + i, 0, 1, 1, (PBYTE)&gpt);

			if (gpt.Header.Signature == GPT_SIGNATURE)
			{
                ULONG_PTR bytesPerEntry;
                ULONG_PTR numberOfEntries;
                ULONG_PTR startOfArray;
                ULONG_PTR sectorCountForPartitionArray;

				UINT8 buffer[1024];
                ULONG_PTR entryNumber = 0;

                ULONG_PTR currentSector, currentEntryPos, entryStartInSector;

                bytesPerEntry = gpt.Header.BytesPerEntry;
                numberOfEntries = gpt.Header.NumberOfPartitionTableEntries;
                startOfArray = gpt.Header.LbaOfPartitionTable;
                sectorCountForPartitionArray = ((UINT64)numberOfEntries * (UINT64)bytesPerEntry - 1) / 512 + 1;
				
                for (currentSector = 0; currentSector < sectorCountForPartitionArray; currentSector++)
				{
					PciIdeDiskIo(Drives + i, 0, startOfArray + currentSector, 2, buffer);

					entryStartInSector = entryNumber * bytesPerEntry % 512;
					
                    for (currentEntryPos = entryStartInSector; currentEntryPos < 512; currentEntryPos += bytesPerEntry)
					{
						GPT_PARTITION_ENTRY* entry = (GPT_PARTITION_ENTRY*)(buffer + currentEntryPos);
						if (!GptCompareGuid(entry->TypeGuid, GPT_EMPTY_TYPE))
						{
							number = VfsAddPartition(Drives + i, entry->LbaPartitionStart, entry->LbaPartitionEnd - entry->LbaPartitionStart - 1, FatVfsInterfaceGetFunctionSet());
							FatInitVfs(VfsGetPointerToVfs(number));
						}
						entryNumber++;
					}
				}
			}
			else
			{
                ULONG_PTR partitionNumber;

				for (partitionNumber = 0; partitionNumber < 4; partitionNumber++)
				{
					MBR_PARTITION_TABLE_ENTRY entry = mbr.mbrtable.TableEntries[partitionNumber];

					if (entry.PartitionSizeInSectors == 0)
						continue;

					number = VfsAddPartition(Drives + i, entry.PartitionStartLBA28, entry.PartitionSizeInSectors, FatVfsInterfaceGetFunctionSet());
					FatInitVfs(VfsGetPointerToVfs(number));
				}
			}

			RegisterPartition(number);

		}
		else
		{
			PrintT("Drive %i not formatted, signature 0x%X\n", i, mbr.mbrtable.MagicNumber);
		}
	}
}