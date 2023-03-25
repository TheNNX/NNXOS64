#include "vfs.h"
#include <nnxalloc.h>
#include <MemoryOperations.h>
#include "fat.h"
#include "fat32.h"
#include <text.h>

#define DEBUG_STATUS PrintT("[%s:%i] %x\n", __FUNCTION__, __LINE__, status);

UINT32 FatVolumeTotalSize(BPB* bpb)
{
	UINT32 volumeTotalSize = bpb->SectorTotSize16;
	if (volumeTotalSize == 0)
		volumeTotalSize = bpb->SectorTotSize32;
	return volumeTotalSize;
}

UINT32 FatFileAllocationTableSize(BPB* bpb)
{
	UINT32 fatSize = bpb->SectorFatSize16;
	if (fatSize == 0)
		fatSize = ((BPB32*)&(bpb->_))->SectorFatSize32;

	return fatSize;
}

UINT32 FatGetClusterEof(BPB* bpb)
{
	if (FatIsFat32(bpb))
		return FAT32_RESERVED_CLUSTER_START;
	if (FatIsFat16(bpb))
		return FAT16_RESERVED_CLUSTER_START;
	return FAT12_RESERVED_CLUSTER_START;
}

BOOL FatIsClusterEof(BPB* bpb, UINT32 cluster)
{
	UINT32 maxCluster = FatGetClusterEof(bpb);
	return cluster >= maxCluster;
}

UINT32 FatCalculateFirstClusterPosition(BPB* bpb)
{
	UINT32 rootDirSectors = ((bpb->RootEntryCount * 32) + (bpb->BytesPerSector - 1)) / bpb->BytesPerSector;
	UINT32 fatSize = FatFileAllocationTableSize(bpb);
	return bpb->SectorReservedSize + bpb->NumberOfFats * fatSize + rootDirSectors;
}

UINT32 FatCalculateFatClusterCount(BPB* bpb)
{
	UINT32 volumeTotalSize = FatVolumeTotalSize(bpb);
	UINT32 dataSectorSize = volumeTotalSize - FatCalculateFirstClusterPosition(bpb);

	return dataSectorSize / bpb->SectorsPerCluster;
}

BOOL FatIsFat32(BPB* bpb)
{
	return bpb->RootEntryCount == 0;
}

BOOL FatIsFat16(BPB* bpb)
{
	if (FatIsFat32(bpb))
		return FALSE;

	return FatCalculateFatClusterCount(bpb) > 4084;
}

BOOL FatIsFat12(BPB* bpb)
{
	return !(FatIsFat16(bpb) || FatIsFat32(bpb));
}

UINT32 FatLocateMainFat(BPB* bpb)
{
	return bpb->SectorReservedSize;
}

UINT32 FatGetRootCluster(BPB* bpb)
{
	return FatIsFat32(bpb) ? (((BPB_EXT_FAT32*)&(bpb->_))->FirstAccessibleCluster) : 0;
}

UINT32 FatLocateNthFat(BPB* bpb, UINT32 n)
{
	return (bpb->NumberOfFats > n) ? (bpb->SectorReservedSize + FatFileAllocationTableSize(bpb) * n) : 0;
}

VOID Fat32WriteFatEntry(BPB* bpb, VFS* vfs, UINT32 n, BYTE* sectorData, UINT32 value)
{
	UINT32 entriesPerSector = bpb->BytesPerSector / 4;
	((UINT32*) sectorData)[n % entriesPerSector] = value;
}

VOID Fat16WriteFatEntry(BPB* bpb, VFS* vfs, UINT32 n, BYTE* sectorData, UINT16 value)
{
	UINT32 entriesPerSector = bpb->BytesPerSector / 2;
	((UINT16*) sectorData)[n % entriesPerSector] = value;
}

/**

TODO: Check if it actually works.

**/
VOID Fat12WriteFatEntry(BPB* bpb, VFS* vfs, UINT32 n, BYTE* sectorsData, UINT16 value)
{
	UINT32 byteOffset = (n * 3) % bpb->BytesPerSector;
	sectorsData += byteOffset;
	*((UINT16*) sectorsData) &= ~0xfff;
	*((UINT16*) sectorsData) |= value;
}


VFS_STATUS FatWriteFatEntryInternal(BPB* bpb, VFS* filesystem, UINT32 n, BYTE* sectorsData, UINT32* currentSector, UINT32 entry)
{
	VFS_STATUS status;
	UINT32 mainFAT;

	FatReadFatEntry(bpb, filesystem, n, sectorsData, currentSector);
	mainFAT = FatLocateMainFat(bpb);
	if (FatIsFat12(bpb))
	{
		Fat12WriteFatEntry(bpb, filesystem, n, sectorsData, (UINT16) (entry & 0xfff));
		if (status = VfsWriteSector(filesystem, (SIZE_T)(*currentSector) + 1LL, sectorsData + bpb->BytesPerSector))
			return VFS_ERR_READONLY;
	}
	else if (FatIsFat32(bpb))
	{
		Fat32WriteFatEntry(bpb, filesystem, n, sectorsData, entry);
	}
	else
	{
		Fat16WriteFatEntry(bpb, filesystem, n, sectorsData, (UINT16) entry);
	}

	if (status = VfsWriteSector(filesystem, *currentSector, sectorsData))
		return VFS_ERR_READONLY;

	return 0;
}

VFS_STATUS FatWriteFatEntry(BPB* bpb, VFS* filesystem, UINT32 n, BYTE* sectorsData, UINT32* currentSector, UINT32 entry)
{
	BOOL manualAllocation = FALSE;
	UINT32 dummy = 0;
	VFS_STATUS status;

	if (currentSector == 0 || sectorsData == 0)
	{
		manualAllocation = TRUE;
		currentSector = &dummy;
		sectorsData = NNXAllocatorAlloc((SIZE_T)bpb->BytesPerSector * 2LL);
	}
	status = FatWriteFatEntryInternal(bpb, filesystem, n, sectorsData, currentSector, entry);

	if (manualAllocation)
	{
		NNXAllocatorFree(sectorsData);
	}

	return status;
}

UINT32 Fat12ReadFatEntry(BPB* bpb, VFS* filesystem, UINT32 mainFAT, BYTE* sectorsData, UINT32* currentSector, UINT32 n)
{
	UINT16 bytesContainingTheEntry;
	UINT32 entryBitOffset = n * 12;
	UINT32 entryByteOffset = entryBitOffset / 8;

	UINT32 relativeLowByteOffset = entryByteOffset % bpb->BytesPerSector;
	UINT32 desiredLowSector = entryByteOffset / bpb->BytesPerSector + mainFAT;

	if (*currentSector != desiredLowSector)
	{
		VFS_STATUS status = 0;
		status |= VfsReadSector(filesystem, (SIZE_T)desiredLowSector, sectorsData);
		status |= VfsReadSector(filesystem, (SIZE_T)desiredLowSector + 1LL, sectorsData + bpb->BytesPerSector);

		if (status)
		{
			return 0;
		}

		*currentSector = desiredLowSector;
	}

	bytesContainingTheEntry = *((UINT16*) (sectorsData + relativeLowByteOffset));

	if (entryBitOffset % 8)
	{
		bytesContainingTheEntry = bytesContainingTheEntry << 4;
	}

	return bytesContainingTheEntry & 0xfff0;
}

UINT32 Fat16Or32ReadFatEntry(BPB* bpb, VFS* filesystem, UINT32 mainFAT, BYTE* sectorsData, UINT32* currentSector, UINT32 n)
{
	UINT32 entryByteOffset = n * 2 * (FatIsFat32(bpb) + 1);
	UINT32 relativeByteOffset = entryByteOffset % bpb->BytesPerSector;
	UINT32 desiredLowSector = entryByteOffset / bpb->BytesPerSector + mainFAT;

	if (*currentSector != desiredLowSector)
	{
		VFS_STATUS status;
		if (status = VfsReadSector(filesystem, desiredLowSector, sectorsData))
		{
			return 0;
		}

		*currentSector = desiredLowSector;
	}

	if (FatIsFat16(bpb))
		return (UINT32) (*((UINT16*) (sectorsData + relativeByteOffset)));
	else
		return (UINT32) (*((UINT32*) (sectorsData + relativeByteOffset)));
}

UINT32 FatReadFatEntry(BPB* bpb, VFS* filesystem, UINT32 n, BYTE* sectorsData, UINT32* currentSector)
{
	UINT32 mainFAT = FatLocateMainFat(bpb);
	UINT32 entry = 0;
	int altCurSec = 0;
	if (currentSector == 0 || sectorsData == 0)
	{
		currentSector = &altCurSec;
		sectorsData = NNXAllocatorAlloc((SIZE_T)bpb->BytesPerSector * 2LL);
	}

	if (FatIsFat32(bpb) || FatIsFat16(bpb))
	{
		entry = Fat16Or32ReadFatEntry(bpb, filesystem, mainFAT, sectorsData, currentSector, n);
	}
	else
	{
		entry = Fat12ReadFatEntry(bpb, filesystem, mainFAT, sectorsData, currentSector, n);
	}

	if (entry == 0)
	{
		if (currentSector == &altCurSec)
		{
			NNXAllocatorFree(sectorsData);
		}
		return 0;
	}

	if (currentSector == &altCurSec)
	{
		NNXAllocatorFree(sectorsData);
	}

	return entry;
}

BOOL FatIsFree(UINT32 n, BPB* bpb, VFS* filesystem, BYTE* sectorsData, UINT32* currentSector)
{
	return !FatReadFatEntry(bpb, filesystem, n, sectorsData, currentSector);
}

VOID FatInitVfs(VFS* partition)
{
	BPB _bpb = { 0 }, * bpb = &_bpb;
	FAT_FILESYSTEM_SPECIFIC_VFS_DATA *pFFsSD;
	VfsReadSector(partition, 0, (UCHAR*)bpb);
	partition->FilesystemSpecificData = NNXAllocatorAlloc(sizeof(FAT_FILESYSTEM_SPECIFIC_VFS_DATA));
	pFFsSD = partition->FilesystemSpecificData;
	pFFsSD->CachedFatSector = NNXAllocatorAlloc(bpb->BytesPerSector);
	pFFsSD->CachedFatSectorNumber = -1;
}

UINT32 FatFollowClusterChain(BPB* bpb, VFS* filesystem, UINT32 cluster)
{
	return FatReadFatEntry(bpb, filesystem, cluster,
		((FAT_FILESYSTEM_SPECIFIC_VFS_DATA*) filesystem->FilesystemSpecificData)->CachedFatSector,
						   &(((FAT_FILESYSTEM_SPECIFIC_VFS_DATA*) filesystem->FilesystemSpecificData)->CachedFatSectorNumber));
}

/**
	WARNING: not recommended for cluster sizes > 4KiB (NNXAllocator cannot allocate memory above 4KB page size)
	Recommended way of reading clusters is to read each sector individually using FatReadSectorOfCluster
**/
VFS_STATUS FatReadCluster(BPB* bpb, VFS* filesystem, UINT32 clusterIndex, BYTE* data)
{
	int sectorIndex;
	UINT32 firstSectorOfCluster = bpb->SectorsPerCluster * clusterIndex + FatCalculateFirstClusterPosition(bpb) - 2 * bpb->SectorsPerCluster;
	
	for (sectorIndex = 0; sectorIndex < bpb->SectorsPerCluster; sectorIndex++)
	{
		VFS_STATUS status = VfsReadSector(filesystem, (SIZE_T)firstSectorOfCluster + (SIZE_T)sectorIndex, data);
		
		if (status)
			return status;
		
		data += bpb->BytesPerSector;
	}
	return 0;
}

VFS_STATUS FatReadSectorOfCluster(BPB* bpb, VFS* filesystem, UINT32 clusterIndex, UINT32 sectorIndex, BYTE* data)
{
	UINT32 firstSectorOfCluster = bpb->SectorsPerCluster * clusterIndex + FatCalculateFirstClusterPosition(bpb) - 2 * bpb->SectorsPerCluster;

	VFS_STATUS status = VfsReadSector(filesystem, (SIZE_T)firstSectorOfCluster + (SIZE_T)sectorIndex, data);
	if (status)
	{
		return status;
	}

	return 0;
}

VFS_STATUS FatWriteSectorOfCluster(BPB* bpb, VFS* filesystem, UINT32 clusterIndex, UINT32 sectorIndex, BYTE* data)
{
	UINT32 firstSectorOfCluster = bpb->SectorsPerCluster * clusterIndex + FatCalculateFirstClusterPosition(bpb) - 2 * bpb->SectorsPerCluster;

	VFS_STATUS status = VfsWriteSector(filesystem, (SIZE_T)firstSectorOfCluster + (SIZE_T)sectorIndex, data);
	if (status)
	{
		return status;
	}

	return 0;
}

void FatCopyNameFromEntry(FAT_DIRECTORY_ENTRY* entry, char* dst, int* endName, int* endExt)
{
	int endOfEntryName = 0;

	*endName = 8;
	for (; (*endName) > 0; (*endName)--)
	{
		if (entry->Filename[(*endName) - 1] != ' ')
			break;
	}

	*endExt = 3;
	for (; (*endExt) > 0; (*endExt)--)
	{
		if (entry->FileExtension[(*endExt) - 1] != ' ')
			break;
	}

	for (int index = 0; index < (*endName); index++)
	{
		dst[index] = entry->Filename[index];
	}
}

BOOL FatCompareName(FAT_DIRECTORY_ENTRY* entry, const char* filename)
{
	char entryName[13] = { 0 };

	int endExt, endName;
	FatCopyNameFromEntry(entry, entryName, &endName, &endExt);

	if (endExt != 0)
	{
		entryName[endName] = '.';
		for (int index = 0; index < endExt; index++)
		{
			entryName[endName + 1 + index] = entry->FileExtension[index];
		}
	}

	int i = 0;
	while (filename[i])
	{
		if (ToUppercase(filename[i]) != ToUppercase(entryName[i]))
			return false;
		i++;
	}

	return true;
}

BOOL FatIsFileOrDir(FAT_DIRECTORY_ENTRY* sectorData)
{
	if (sectorData->Filename[0] == 0x0)
	{
		return false;
	}
	else if (sectorData->Filename[0] == 0xe5)
	{
		return false;
	}
	else if (sectorData->FileAttributes & FAT_VOLUME_ID)
	{
		return false;
	}
	return true;
}

VFS_STATUS FatSearchForFileInDirectory(FAT_DIRECTORY_ENTRY* sectorData, BPB* bpb, VFS* filesystem, const char * name, FAT_DIRECTORY_ENTRY* output)
{
	SIZE_T entryIndex;

	for (entryIndex = 0; entryIndex < (bpb->BytesPerSector / 32); entryIndex++)
	{
		if (!FatIsFileOrDir(sectorData + entryIndex))
		{
			continue;
		}
		else if (FatCompareName(sectorData + entryIndex, name))
		{
			*output = sectorData[entryIndex];
			return 0;
		}
	}

	return VFS_ERR_FILE_NOT_FOUND; //scan next entry
}

UINT32 FatGetFirstClusterOfFile(BPB* bpb, FAT_DIRECTORY_ENTRY* dirEntry)
{
	if (dirEntry == 0)
	{
		return 0xFFFFFFFF;
	}
	return dirEntry->LowCluster | (FatIsFat32(bpb) ? (dirEntry->HighCluster << 16) : 0);
}

VFS_STATUS FatCopyFirstFilenameFromPath(const char* path, char* filenameCopy)
{
	SIZE_T slash = FindFirstSlash(path);
	SIZE_T i, endI;

	if (slash == -1 && FindCharacterFirst(path, -1, 0) <= 12)
	{
		MemCopy(filenameCopy, (void*)path, FindCharacterFirst(path, -1, 0));
		filenameCopy[FindCharacterFirst(path, -1, 0)] = 0;
		return 0;
	}

	if (slash > 12)
	{
		return VFS_ERR_INVALID_PATH;
	}

	if (path[slash + 1] == 0)
		slash = 0;

	if (slash != 0)
	{
		for (i = 0; i < slash; i++)
		{
			filenameCopy[i] = path[i];
			filenameCopy[i + 1] = 0;
		}
	}

	for (endI = 13; endI > 0; endI--)
	{
		if (filenameCopy[endI - 1] == '.')
		{
			endI--;
			break;
		}
	}

	if (endI > 9 || (endI == 0 && slash > 8))
	{
		return VFS_ERR_INVALID_FILENAME;
	}

	return 0;
}

BOOL FatWriteDirectoryEntryToSectors(FAT_DIRECTORY_ENTRY* sectorData, BPB* bpb, VFS* filesystem, char* filename, FAT_DIRECTORY_ENTRY* fileDir)
{
	SIZE_T entryIndex;

	for (entryIndex = 0; entryIndex < (bpb->BytesPerSector / 32); entryIndex++)
	{
		if (sectorData[entryIndex].Filename[0] == 0x0
			|| FatCompareName(sectorData + entryIndex, filename))
		{
			sectorData[entryIndex] = *fileDir;
			return TRUE;
		}
	}

	return FALSE;
}

VFS_STATUS FatReccursivlyFindDirectoryEntry(BPB* bpb, VFS* filesystem, UINT32 parentDirectoryCluster, const char* path, FAT_DIRECTORY_ENTRY* fileDir)
{
	SIZE_T slash;
	char filenameCopy[13];
	FAT_DIRECTORY_ENTRY dirEntry;
	VFS_STATUS status;
	PBYTE sectorData;
	UINT32 i;

	while (*path == '\\' || *path == '/') /* skip all initial \ or / */
		path++;
	
	slash = FindFirstSlash(path);
	status = FatCopyFirstFilenameFromPath(path, filenameCopy);

	if (status)
	{
		return status;
	}

	sectorData = NNXAllocatorAlloc(bpb->BytesPerSector);

	/* we need to find first directory entry manually */
	if (FatIsFat32(bpb) == false && (parentDirectoryCluster == 0 || parentDirectoryCluster == 0xFFFFFFFF))
	{
		UINT32 fatSize = FatFileAllocationTableSize(bpb);
		UINT32 rootDirStart = bpb->SectorReservedSize + bpb->NumberOfFats * fatSize;
		UINT32 rootDirSectors = ((bpb->RootEntryCount * 32) + (bpb->BytesPerSector - 1)) / bpb->BytesPerSector;
		FAT_DIRECTORY_ENTRY directory;
		
		for (i = 0; i < rootDirSectors; i++)
		{
			VfsReadSector(filesystem, (SIZE_T)rootDirStart + (SIZE_T)i, sectorData);

			status = FatSearchForFileInDirectory((FAT_DIRECTORY_ENTRY*)sectorData, bpb, filesystem, filenameCopy, &directory);

			if ((status) && (i == rootDirSectors - 1))
			{
				rootDirSectors = 0;
				break;
			}
			if (!status)
			{
				break;
			}

		}
		NNXAllocatorFree(sectorData);
		if (rootDirSectors == 0)
		{
			return VFS_ERR_FILE_NOT_FOUND;
		}
		if (slash != -1)
		{
			if ((directory.FileAttributes & FAT_DIRECTORY) == 0)
			{
				return VFS_ERR_NOT_A_DIRECTORY;
			}
			return FatReccursivlyFindDirectoryEntry(bpb, filesystem, directory.LowCluster, path + slash + 1, fileDir);
		}
		else
		{
			*fileDir = directory;
			return 0;
		}
	}

	if (parentDirectoryCluster == 0xFFFFFFFF)
	{
		parentDirectoryCluster = FatGetRootCluster(bpb);
	}

	while (!FatIsClusterEof(bpb, parentDirectoryCluster))
	{
		UINT32 sectorIndex;

		for (sectorIndex = 0; sectorIndex < bpb->SectorsPerCluster; sectorIndex++)
		{
			status = FatReadSectorOfCluster(bpb, filesystem, parentDirectoryCluster, sectorIndex, sectorData);

			if (status)
				return status;

			status = FatSearchForFileInDirectory((FAT_DIRECTORY_ENTRY*)sectorData, bpb, filesystem, filenameCopy, &dirEntry);

			if (status == VFS_ERR_FILE_NOT_FOUND)
				continue;
			else if (status)
			{
				NNXAllocatorFree(sectorData);
				return status;
			}

			if (slash != -1)
			{
				UINT32 fisrtClusterOfFile;

				if ((dirEntry.FileAttributes & FAT_DIRECTORY) == 0)
				{
					NNXAllocatorFree(sectorData);
					return VFS_ERR_NOT_A_DIRECTORY;
				}

				fisrtClusterOfFile = FatGetFirstClusterOfFile(bpb, &dirEntry);

				NNXAllocatorFree(sectorData);

				return FatReccursivlyFindDirectoryEntry(bpb, filesystem, fisrtClusterOfFile, path + slash + 1, fileDir);
			}
			else
			{
				*fileDir = dirEntry;
				NNXAllocatorFree(sectorData);

				return 0;
			}
		}
		parentDirectoryCluster = FatFollowClusterChain(bpb, filesystem, parentDirectoryCluster);
	}
	NNXAllocatorFree(sectorData);
	return VFS_ERR_FILE_NOT_FOUND;
}

UINT32 FatScanFree(VFS* filesystem)
{
	BPB _bpb = { 0 }, *bpb = &_bpb;
	VfsReadSector(filesystem, 0, (UCHAR*)bpb);
	UINT32 fatSize = FatFileAllocationTableSize(bpb);
	bool isFAT16 = FatIsFat16(bpb);

	UINT8 *sectorsData = NNXAllocatorAlloc(((bpb->RootEntryCount == 0 || ((bpb->RootEntryCount != 0) && isFAT16)) ? 1ULL : 2ULL) * (SIZE_T)bpb->BytesPerSector);
	UINT32 currentSector = 0;
	UINT32 clusterCount = FatCalculateFatClusterCount(bpb);
	UINT32 freeClusters = 0;

	for (UINT32 currentEntry = 1; currentEntry < clusterCount + 2; currentEntry++)
	{
		freeClusters += FatIsFree(currentEntry, bpb, filesystem, sectorsData, &currentSector);
	}

	NNXAllocatorFree(sectorsData);
	return freeClusters;
}

UINT32 FatFindFreeCluster(BPB* bpb, VFS* vfs)
{
	UINT32 clusterCount = FatCalculateFatClusterCount(bpb);
	UINT32 currentSector = 0;
	UINT8 *sectorsData = NNXAllocatorAlloc((FatIsFat12(bpb) ? 2ULL : 1ULL) * (SIZE_T)bpb->BytesPerSector);

	for (UINT32 currentEntry = 1; currentEntry < clusterCount + 2; currentEntry++)
	{
		if (FatIsFree(currentEntry, bpb, vfs, sectorsData, &currentSector))
		{
			NNXAllocatorFree(sectorsData);
			return currentEntry;
		}
	}

	NNXAllocatorFree(sectorsData);
	return 0xFFFFFFFF;
}


VFS_STATUS FatReadSectorFromFile(BPB* bpb, VFS* vfs, FAT_DIRECTORY_ENTRY* file, UINT32 offsetSector, PVOID output)
{
	VFS_STATUS status = 0;
	UINT32 curCluster;
	SIZE_T index;
	UINT32 clusterNumberPreceding = offsetSector / bpb->SectorsPerCluster;
	UINT32 curSector = offsetSector % bpb->SectorsPerCluster;
	UINT8 buffer[4096] = { 0 };

	if (file == NULL)
	{
		if (FatIsFat32(bpb))
		{
			curCluster = FatGetRootCluster(bpb);
		}
		else
		{
			UINT32 fatSize = FatFileAllocationTableSize(bpb);
			return VfsReadSector(vfs, (SIZE_T)offsetSector + (SIZE_T)bpb->SectorReservedSize + (SIZE_T)bpb->NumberOfFats * (SIZE_T)fatSize, output);
		}
	}
	else
	{
		curCluster = FatGetFirstClusterOfFile(bpb, file);
	}

	curCluster = FatFollowClusterChainToAPoint(bpb, vfs, curCluster, clusterNumberPreceding);


	if (status = FatReadSectorOfCluster(bpb, vfs, curCluster, curSector, buffer))
	{
		return status;
	}

	for (index = 0; index < bpb->BytesPerSector; index++)
	{
		((UINT8*) output)[index] = buffer[index];
	}

	return 0;
}

VFS_STATUS FatWriteSectorsToFile(BPB* bpb, VFS* vfs, FAT_DIRECTORY_ENTRY* file, UINT32 index, PVOID input, UINT32 sectorsCount)
{
	UINT32 currentCluster;
	VFS_STATUS status;
	UINT32 firstSector = index;
	UINT32 lastSector = index + sectorsCount - 1;
	UINT32 firstCluster = firstSector / bpb->SectorsPerCluster;
	UINT32 lastCluster = lastSector / bpb->SectorsPerCluster;
	UINT32 currentClusterIndex = firstCluster;
	SIZE_T inputOffset = 0;

	if (file == NULL)
	{
		if (FatIsFat32(bpb))
		{
			currentCluster = FatGetRootCluster(bpb);
		}
		else
		{
			UINT32 fatSize = FatFileAllocationTableSize(bpb);
			UINT32 i;

			for (i = 0; i < sectorsCount; i++)
			{
				if (status = VfsWriteSector(vfs, (SIZE_T)index + (SIZE_T)bpb->SectorReservedSize + (SIZE_T)bpb->NumberOfFats * (SIZE_T)fatSize, input))
					return status;
			}
			return 0;
		}
	}
	else
	{
		currentCluster = FatGetFirstClusterOfFile(bpb, file);
	}

	currentCluster = FatFollowClusterChainToAPoint(bpb, vfs, currentCluster, firstCluster);

	while (currentClusterIndex <= lastCluster)
	{
		if (FatIsClusterEof(bpb, currentCluster))
		{
			status = VFS_ERR_EOF;
			DEBUG_STATUS;
			return VFS_ERR_EOF;
		}

		UINT32 i;

		for (i = 0; i < bpb->SectorsPerCluster; i++)
		{
			if (i + currentClusterIndex * bpb->SectorsPerCluster > lastSector)
				return 0;
			if (i + currentClusterIndex * bpb->SectorsPerCluster >= firstSector)
			{
				if (status = FatWriteSectorOfCluster(bpb, vfs, currentCluster, i, (PBYTE)((UINT64) input + inputOffset)))
				{
					DEBUG_STATUS;
				}

				inputOffset += bpb->BytesPerSector;
			}
		}

		currentClusterIndex++;
		currentCluster = FatFollowClusterChain(bpb, vfs, currentCluster);
	}

	return 0;
}

VFS_STATUS FatWriteFile(BPB* bpb, VFS* vfs, FAT_DIRECTORY_ENTRY* file, UINT64 offset, SIZE_T size, PVOID input, UINT32* writtenBytes)
{
	UINT32 startSector = (UINT32)(offset / bpb->BytesPerSector);
	UINT32 endSector = (UINT32)(offset + size - 1) / bpb->BytesPerSector;
	UINT32 sectorOffset = (UINT32)(offset % bpb->BytesPerSector);
	UINT32 lastSectorEndOffset = (offset + size - 1) % bpb->BytesPerSector;
	UINT8 *temporarySectorData = NNXAllocatorAlloc(bpb->BytesPerSector);
	UINT32 i, localWrittenBytes = 0;

	if (file == 0)
	{
		NNXAllocatorFree(temporarySectorData);
		return VFS_ERR_FILE_NOT_FOUND;
	}

	if (writtenBytes == 0)
		writtenBytes = &localWrittenBytes;
	*writtenBytes = 0;

	if (!lastSectorEndOffset)
		lastSectorEndOffset = bpb->BytesPerSector - 1;

	for (i = startSector; i <= endSector; i++)
	{
		VFS_STATUS status;
		if (i == endSector || (i == startSector && sectorOffset))
		{
			UINT32 lowerBound, upperBound, j;
			FatReadSectorFromFile(bpb, vfs, file, i, temporarySectorData);

			lowerBound = (i == startSector) ? sectorOffset : 0;
			upperBound = (i == endSector) ? lastSectorEndOffset + 1 : bpb->BytesPerSector;

			for (j = 0; j < upperBound - lowerBound; j++)
			{
				temporarySectorData[j + lowerBound] = ((UINT8*) input)[(*writtenBytes)++];
			}
			status = FatWriteSectorsToFile(bpb, vfs, file, i, temporarySectorData, 1);
			if (status)
				DEBUG_STATUS;

		}
		else
		{
			status = FatWriteSectorsToFile(bpb, vfs, file, i, ((UINT8*) input) + *writtenBytes, 1);
			if (status)
				DEBUG_STATUS;
			(*writtenBytes) += 512;
		}


		if (status)
		{
			NNXAllocatorFree(temporarySectorData);
			return status;
		}
	}

	NNXAllocatorFree(temporarySectorData);
	return 0;
}

VFS_STATUS FatReadFile(BPB* bpb, VFS* vfs, FAT_DIRECTORY_ENTRY* file, UINT32 offset, UINT32 size, PVOID output, UINT32* readBytes)
{
	UINT32 startSector = offset / bpb->BytesPerSector;
	UINT32 endSector = (offset + size - 1) / bpb->BytesPerSector;
	UINT8 buffer[4096] = { 0 };
	UINT32 localReadBytes = 0;
	UINT32 i;

	if (size == 0)
		return 0;

	if (readBytes == 0)
		readBytes = &localReadBytes;

	*readBytes = 0;

	for (i = startSector; i <= endSector; i++)
	{
		UINT32 lowReadLimit = 0, highReadLimit = bpb->BytesPerSector;
		VFS_STATUS status;
		UINT32 j;

		if (i == startSector)
		{
			lowReadLimit = offset % bpb->BytesPerSector;
		}
		if (i == endSector)
		{
			highReadLimit = (offset + size) % bpb->BytesPerSector;
			if (!highReadLimit)
				highReadLimit = bpb->BytesPerSector;
		}

		if (lowReadLimit != 0 || highReadLimit != bpb->BytesPerSector)
		{
			if (status = FatReadSectorFromFile(bpb, vfs, file, i, buffer))
			{
				return status;
			}

			for (j = lowReadLimit; j < highReadLimit; j++)
			{
				((UINT8*) output)[(*readBytes)++] = buffer[j];
			}
		}
		else
		{
			if (status = FatReadSectorFromFile(bpb, vfs, file, i, ((UINT8*) output) + *readBytes))
			{
				return status;
			}
			*readBytes += 512;
		}

	}

	return 0;
}

VFS_STATUS FatVfsInterfaceGetDirectoryEntry(VFS* filesystem, const char * path, FAT_DIRECTORY_ENTRY* theFile, BPB* bpb)
{
	VfsReadSector(filesystem, 0, (UCHAR*)bpb);
	UINT32 rootCluster = FatGetRootCluster(bpb);
	VFS_STATUS status = FatReccursivlyFindDirectoryEntry(bpb, filesystem, rootCluster, path, theFile);
	return status;
}

BOOL FatVfsInterfaceCheckIfFileExists(VFS* filesystem, const char* path)
{
	FAT_DIRECTORY_ENTRY theFile;
	BPB _bpb = { 0 }, *bpb = &_bpb;
	VFS_STATUS status = FatVfsInterfaceGetDirectoryEntry(filesystem, path, &theFile, bpb);
	return !status;
}

BOOL FatCompareEntries(FAT_DIRECTORY_ENTRY* entry1, FAT_DIRECTORY_ENTRY* entry2)
{
	UINT32 i;

	for (i = 0; i < 8; i++)
	{
		if (i < 3)
		{
			if (entry1->FileExtension[i] != entry2->FileExtension[i])
				return FALSE;
		}
		if (entry1->Filename[i] != entry2->Filename[i])
			return FALSE;
	}
	return TRUE;
}

VFS_STATUS FatChangeDirectoryEntry(BPB* bpb, VFS* filesystem, FAT_DIRECTORY_ENTRY* parent,
							   FAT_DIRECTORY_ENTRY* fileEntry, FAT_DIRECTORY_ENTRY* desiredFileEntry)
{
	unsigned char* buffer = NNXAllocatorAlloc(bpb->BytesPerSector);
	VFS_STATUS status = 0;
	UINT32 offset = 0;
	BOOL done = FALSE;

	if (FatGetFirstClusterOfFile(bpb, parent) == 0)
	{
		NNXAllocatorFree(buffer);
		return VFS_ERR_EOF;
	}

	while (!done && ((status = FatReadSectorFromFile(bpb, filesystem, parent, offset, buffer)) == 0))
	{
		FAT_DIRECTORY_ENTRY* currentEntry;

		if (status)
		{
			DEBUG_STATUS;
			break;
		}

		for (currentEntry = (FAT_DIRECTORY_ENTRY*)buffer;
			 currentEntry < ((FAT_DIRECTORY_ENTRY*) buffer) + (bpb->BytesPerSector) / sizeof(FAT_DIRECTORY_ENTRY);
			 currentEntry++)
		{
			if (((fileEntry->Filename[0] == FAT_FILE_DELETED || fileEntry->Filename[0] == 0) &&
				(currentEntry->Filename[0] == FAT_FILE_DELETED || currentEntry->Filename[0] == 0))
				|| FatCompareEntries(fileEntry, currentEntry))
			{
				*currentEntry = *desiredFileEntry;
				if (parent)
					parent->FileSize = 0xFFFFFFFF; // bypass the size checks (if any)
				status = FatWriteSectorsToFile(bpb, filesystem, parent, offset, buffer, 1);
				if (status)
				{
					DEBUG_STATUS;
				}
				done = TRUE;
				break;
			}

		}

		offset++;
	}

	NNXAllocatorFree(buffer);
	return status;
}

VFS_STATUS FatAddDirectoryEntry(BPB* bpb, VFS* filesystem, FAT_DIRECTORY_ENTRY* parent, FAT_DIRECTORY_ENTRY* fileEntry)
{
	FAT_DIRECTORY_ENTRY empty = { 0 };
	return FatChangeDirectoryEntry(bpb, filesystem, parent, &empty, fileEntry);
}

VFS_STATUS FatVfsInterfaceCreateFile(VFS* filesystem, const char* path)
{
	BPB _bpb = { 0 }, * bpb = &_bpb;
	FAT_DIRECTORY_ENTRY fileEntry = { 0 };
	SIZE_T borderPoint = GetFileNameAndExtensionFromPath(path, fileEntry.Filename, fileEntry.FileExtension);
	FAT_DIRECTORY_ENTRY parent = { 0 };
	VFS_STATUS status;

	if (FatVfsInterfaceCheckIfFileExists(filesystem, path))
		return VFS_ERR_FILE_ALREADY_EXISTS;

	if (borderPoint)
	{
		char *parentPath = NNXAllocatorAlloc(borderPoint);
		UINT32 parentCluster;

		for (UINT32 i = 0; i < borderPoint - 1; i++)
		{
			parentPath[i] = path[i];
		}

		parentPath[borderPoint - 1] = 0;
		FatVfsInterfaceGetDirectoryEntry(filesystem, parentPath, &parent, bpb);
		
		if (parent.HighCluster == 0 && parent.LowCluster == 0)
		{
			UINT32 cluster = FatFindFreeCluster(bpb, filesystem);

			if (status = FatWriteFatEntry(bpb, filesystem, cluster, 0, 0, FatGetClusterEof(bpb)))
			{
				NNXAllocatorFree(parentPath);
				DEBUG_STATUS;
				return status;
			}
		}
		
		parentCluster = FatGetFirstClusterOfFile(bpb, &parent);
		status = FatAddDirectoryEntry(bpb, filesystem, &parent, &fileEntry);
		
		if (status == VFS_ERR_EOF)
		{
			status = FatIncreaseClusterChainLengthBy(bpb, filesystem, parentCluster, 1);
			
			if (status == 0)
			{
				status = FatAddDirectoryEntry(bpb, filesystem, &parent, &fileEntry);
				if (status)
					DEBUG_STATUS;
			}
			else
			{
				status = VFS_ERR_NOT_ENOUGH_ROOM_FOR_WRITE;
				DEBUG_STATUS;
			}
		}

		NNXAllocatorFree(parentPath);
		return status;
	}
	else
	{
		if (!(status = VfsReadSector(filesystem, 0, (PBYTE)bpb)))
		{
			if (status = FatAddDirectoryEntry(bpb, filesystem, 0, &fileEntry))
				return status;
		}
		else
		{
			return status;
		}
	}

	return STATUS_SUCCESS;
}

VFS_FILE* FatVfsInterfaceOpenFile(VFS* vfs, const char * path)
{
	BPB _bpb = { 0 }, *bpb = &_bpb;
	FAT_DIRECTORY_ENTRY direntry;
	VFS_STATUS status;

	if (FatVfsInterfaceCheckIfFileExists(vfs, path) == false)
		return 0;

	if (status = FatVfsInterfaceGetDirectoryEntry(vfs, path, &direntry, bpb))
	{
		return 0;
	}

	VFS_FILE* file = VfsAllocateVfsFile(vfs, path);
	file->FileSize = direntry.FileSize;
	return file;
}

VOID FatVfsInterfaceCloseFile(VFS_FILE* file)
{
	VfsDeallocateVfsFile(file);
}

VFS_STATUS FatVfsInterfaceRecreateDeletedFile(VFS_FILE* file)
{
	return FatVfsInterfaceCreateFile(file->Filesystem, file->Path);
}

VFS_STATUS FatVfsInterfaceDeleteFileFromPath(VFS* filesystem, char* path)
{
	BPB _bpb = { 0 }, * bpb = &_bpb;
	VFS_STATUS status;
	FAT_DIRECTORY_ENTRY theFile, parentFile;
	SIZE_T parentPathLength = GetParentPathLength(path) + 1;

	if (status = FatVfsInterfaceGetDirectoryEntry(filesystem, path, &theFile, bpb))
	{
		return status;
	}

	if (parentPathLength != 0)
	{
		char* parentPath = NNXAllocatorAlloc(parentPathLength);
		GetParentPath(path, parentPath);

		if (status = FatVfsInterfaceGetDirectoryEntry(filesystem, parentPath, &parentFile, bpb))
		{
			NNXAllocatorFree(parentPath);
			return status;
		}

		if (status = FatDeleteFile(bpb, filesystem, &parentFile, path + parentPathLength))
		{
			NNXAllocatorFree(parentPath);
			return status;
		}
		NNXAllocatorFree(parentPath);
	}
	else
	{
		if (status = FatDeleteFile(bpb, filesystem, 0LL, path))
		{
			return status;
		}
	}

	return 0;
}


VFS_STATUS FatVfsInterfaceDeleteFile(VFS_FILE* file)
{
	return FatVfsInterfaceDeleteFileFromPath(file->Filesystem, file->Path);
}

VFS_STATUS FatVfsInterfaceDeleteAndCloseFile(VFS_FILE* file)
{
	VFS_STATUS status = FatVfsInterfaceDeleteFile(file);
	FatVfsInterfaceCloseFile(file);
	return status;
}

VFS_STATUS Checks(VFS_FILE* file, SIZE_T size, VOID* buffer)
{
	if (size == 0)
		return VFS_ERR_ARGUMENT_INVALID;
	if (file == 0)
		return VFS_ERR_ARGUMENT_INVALID;
	if (file->Path == 0)
		return VFS_ERR_INVALID_PATH;
	if (file->Name == 0)
		return VFS_ERR_INVALID_FILENAME;
	if (file->FileSize < file->FilePointer)
		return VFS_ERR_ARGUMENT_INVALID;

	return 0;
}

VFS_STATUS FatVfsInterfaceWriteFile(VFS_FILE* file, SIZE_T size, VOID* buffer)
{
	FAT_DIRECTORY_ENTRY direntry;
	BPB _bpb = { 0 }, *bpb = &_bpb;
	VFS_STATUS status;
	UINT32 written;

	if (status = Checks(file, size, buffer))
	{
		DEBUG_STATUS;
		return status;
	}

	if (status = FatVfsInterfaceGetDirectoryEntry(file->Filesystem, file->Path, &direntry, bpb))
	{
		DEBUG_STATUS;
		return status;
	}

	status = FatWriteFile(bpb, file->Filesystem, &direntry, file->FilePointer, size, buffer, &written);
	file->FilePointer += written;

	if (status)
		DEBUG_STATUS;
	return status;
}

VFS_STATUS FatVfsInterfaceReadFile(VFS_FILE* file, SIZE_T size, VOID* buffer)
{
	FAT_DIRECTORY_ENTRY direntry;
	BPB _bpb = { 0 }, * bpb = &_bpb;
	VFS_STATUS status;
	UINT32 read;

	if (status = Checks(file, size, buffer))
		return status;

	if (status = FatVfsInterfaceGetDirectoryEntry(file->Filesystem, file->Path, &direntry, bpb))
		return status;

	status = FatReadFile(bpb, file->Filesystem, &direntry, (UINT32)file->FilePointer, (UINT32)size, buffer, &read);
	file->FilePointer += read;

	if (status)
		return status;

	return 0;
}

VFS_STATUS FatVfsInterfaceAppendFile(VFS_FILE* file, SIZE_T size, VOID* buffer)
{
	FAT_DIRECTORY_ENTRY direntry;
	BPB _bpb = { 0 }, *bpb = &_bpb;
	VFS_STATUS status;
	UINT32 written;

	if (status = Checks(file, size, buffer))
	{
		PrintT("[%s] Checks failed\n", __FUNCTION__);
		return status;
	}

	if (status = FatVfsInterfaceResizeFile(file, file->FileSize + size))
	{
		PrintT("[%s] Resize failed\n", __FUNCTION__);
		return status;
	}

	if (status = FatVfsInterfaceGetDirectoryEntry(file->Filesystem, file->Path, &direntry, bpb))
	{
		PrintT("[%s] Pseudoopen failed\n", __FUNCTION__);
		return status;
	}

	return FatWriteFile(bpb, file->Filesystem, &direntry, file->FileSize - size, size, buffer, &written);
}

VFS_STATUS FatVfsInterfaceResizeFile(VFS_FILE* file, SIZE_T newsize)
{
	FAT_DIRECTORY_ENTRY parentDirentry;
	BPB _bpb = { 0 }, * bpb = &_bpb;
	VFS_STATUS status;
	SIZE_T parentPathLength;

	VfsReadSector(file->Filesystem, 0, (PBYTE)bpb);

	parentPathLength = GetParentPathLength(file->Path) + 1;

	if (parentPathLength > 0)
	{
		char* parentPath = NNXAllocatorAlloc(parentPathLength);
		GetParentPath(file->Path, parentPath);
		status = FatVfsInterfaceGetDirectoryEntry(file->Filesystem, parentPath, &parentDirentry, bpb);
		if (status)
		{
			PrintT("[%s] Pseudoopen failed\n", __FUNCTION__);
			NNXAllocatorFree(parentPath);
			return status;
		}

		NNXAllocatorFree(parentPath);


		status = FatResizeFile(bpb, file->Filesystem, &parentDirentry, file->Name, newsize);

		if (status)
		{
			PrintT("[%s:%i] Resize failed\n", __FUNCTION__, __LINE__);
			return status;
		}
		file->FileSize = newsize;
	}
	else
	{
		status = FatResizeFile(bpb, file->Filesystem, 0, file->Name, newsize);

		if (status)
		{
			PrintT("[%s:%i] Resize failed\n", __FUNCTION__, __LINE__);
			return status;
		}
		file->FileSize = newsize;
	}

	return 0;
}

VFS_STATUS FatVfsInterfaceChangeDirectoryEntryInternal(VFS* vfs, const char * path, FAT_DIRECTORY_ENTRY* desiredFileEntry, BPB* bpb)
{
	SIZE_T parentPathLength;
	VFS_STATUS status;
	FAT_DIRECTORY_ENTRY fileEntry;

	if (status = FatVfsInterfaceGetDirectoryEntry(vfs, path, &fileEntry, bpb))
		return status;

	parentPathLength = GetParentPathLength(path);
	if (parentPathLength > 0)
	{
		FAT_DIRECTORY_ENTRY parentFile;

		char* parentPath = NNXAllocatorAlloc(parentPathLength + 1);
		GetParentPath(path, parentPath);

		if (status = FatVfsInterfaceGetDirectoryEntry(vfs, parentPath, &parentFile, bpb))
		{
			NNXAllocatorFree(parentPath);
			DEBUG_STATUS;
			return status;
		}

		if (status = FatChangeDirectoryEntry(bpb, vfs, &parentFile, &fileEntry, desiredFileEntry))
		{
			NNXAllocatorFree(parentPath);
			DEBUG_STATUS;
			return status;
		}
		NNXAllocatorFree(parentPath);
	}
	else
	{
		if (status = FatChangeDirectoryEntry(bpb, vfs, 0, &fileEntry, desiredFileEntry))
		{
			DEBUG_STATUS;
			return status;
		}
	}

	return 0;
}

VFS_STATUS FatVfsInterfaceChangeDirectoryEntry(VFS* vfs, const char * path, FAT_DIRECTORY_ENTRY* desiredFileEntry, BPB* bpb)
{
	VFS_STATUS status = FatVfsInterfaceChangeDirectoryEntryInternal(vfs, path, desiredFileEntry, bpb);

	if (status)
	{
		FAT_DIRECTORY_ENTRY file;
		FatVfsInterfaceGetDirectoryEntry(vfs, path, &file, bpb);

		status = FatIncreaseClusterChainLengthBy(bpb, vfs, FatGetFirstClusterOfFile(bpb, &file), 1);
		if (status == 0)
		{
			status = FatVfsInterfaceChangeDirectoryEntryInternal(vfs, path, desiredFileEntry, bpb);
			if (status)
				DEBUG_STATUS;
		}
		else
		{
			status = VFS_ERR_NOT_ENOUGH_ROOM_FOR_WRITE;
			DEBUG_STATUS;
		}
	}

	return 0;
}

VFS_STATUS FatVfsInterfaceChangeFileAttributes(VFS* vfs, const char * path, BYTE attributes)
{
	VFS_STATUS status;
	BPB _bpb = { 0 }, *bpb = &_bpb;
	FAT_DIRECTORY_ENTRY fileEntry;

	if (status = FatVfsInterfaceGetDirectoryEntry(vfs, path, &fileEntry, bpb))
	{
		DEBUG_STATUS;
		return status;
	}

	fileEntry.FileAttributes = attributes;

	if (status = FatVfsInterfaceChangeDirectoryEntry(vfs, path, &fileEntry, bpb))
	{
		DEBUG_STATUS;
		return status;
	}

	return 0;
}

BYTE FatVfsInterfaceGetFileAttributes(VFS* vfs, const char * path)
{
	BPB _bpb = { 0 }, * bpb = &_bpb;
	FAT_DIRECTORY_ENTRY theFile;
	VFS_STATUS status;

	status = FatVfsInterfaceGetDirectoryEntry(vfs, path, &theFile, bpb);
	if (status)
		return 0x00;
	
	return theFile.FileAttributes;
}

VFS_STATUS FatVfsInterfaceCreateDirectory(VFS* vfs, const char * path)
{
	BPB _bpb = { 0 }, *bpb = &_bpb;
	FAT_DIRECTORY_ENTRY fileEntry;
	FAT_DIRECTORY_ENTRY selfEntry, parentEntry;
	SIZE_T parentPathLength, parentCluster;
	VFS_STATUS status;
	SIZE_T i;

	/* Remove trailing slashes, if any*/
	SIZE_T pathLength = FindCharacterFirst(path, -1, 0);

	char* pathCopy = NNXAllocatorAlloc(pathLength + 1);

	pathCopy[pathLength] = 0;
	while (pathLength & (pathCopy[pathLength - 1] == '/' || pathCopy[pathLength - 1] == '\\'))
	{
		pathCopy[pathLength - 1] = 0;
		pathLength--;
	}

	for (i = 0; i < pathLength; i++)
	{
		pathCopy[i] = path[i];
	}
	/* Ignore the original Path, replace the pointer with the pointer to the local (trailing-slashless) copy */
	path = pathCopy;


	if (status = FatVfsInterfaceCreateFile(vfs, pathCopy))
	{
		NNXAllocatorFree(pathCopy);
		DEBUG_STATUS;
		return status;
	}

	if (status = FatVfsInterfaceChangeFileAttributes(vfs, pathCopy, FAT_DIRECTORY))
	{
		NNXAllocatorFree(pathCopy);
		DEBUG_STATUS;
		return status;
	}

	if (status = FatVfsInterfaceGetDirectoryEntry(vfs, pathCopy, &fileEntry, bpb))
	{
		VFS_STATUS status2;
		if (status2 = FatVfsInterfaceChangeFileAttributes(vfs, pathCopy, 0))
		{
			NNXAllocatorFree(pathCopy);
			DEBUG_STATUS;
			return status;
		}
		if (status2 = FatVfsInterfaceDeleteFileFromPath(vfs, pathCopy))
		{
			NNXAllocatorFree(pathCopy);
			DEBUG_STATUS;
			return status;
		}

		NNXAllocatorFree(pathCopy);
		DEBUG_STATUS;
		return status;
	}

	parentEntry = fileEntry;
	selfEntry = fileEntry;

	MemCopy(parentEntry.Filename, "..      ", 8);
	MemCopy(selfEntry.Filename, ".       ", 8);
	MemCopy(parentEntry.FileExtension, "   ", 3);
	MemCopy(selfEntry.FileExtension, "   ", 3);

	parentPathLength = GetParentPathLength(pathCopy);
	if (parentPathLength)
	{
		FAT_DIRECTORY_ENTRY tempParent;
		char* parentPath = NNXAllocatorAlloc(parentPathLength + 1);
		GetParentPath(pathCopy, parentPath);

		if (status = FatVfsInterfaceGetDirectoryEntry(vfs, parentPath, &tempParent, bpb))
		{
			NNXAllocatorFree(parentPath);
			DEBUG_STATUS;
			return status;
		}

		parentCluster = FatGetFirstClusterOfFile(bpb, &tempParent);
		NNXAllocatorFree(parentPath);
	}
	else
	{
		parentCluster = FatGetRootCluster(bpb);
	}

	parentEntry.LowCluster = parentCluster & 0xFFFF;
	parentEntry.HighCluster = (parentCluster & 0xFFFF0000) >> 16;

	FatAddDirectoryEntry(bpb, vfs, &fileEntry, &selfEntry);
	FatAddDirectoryEntry(bpb, vfs, &fileEntry, &parentEntry);

	NNXAllocatorFree(pathCopy);
	if (status)
		DEBUG_STATUS;
	return status;
}

VFS_FILE* FatVfsInterfaceOpenOrCreateFile(VFS* vfs, const char * path)
{
	if (!FatVfsInterfaceCheckIfFileExists(vfs, path))
	{
		if (FatVfsInterfaceCreateFile(vfs, path))
			return 0;
	}

	return FatVfsInterfaceOpenFile(vfs, path);
}

VFS_FUNCTION_SET FatVfsInterfaceGetFunctionSet()
{
	VFS_FUNCTION_SET functionSet = { 0 };
	functionSet.CheckIfFileExists = FatVfsInterfaceCheckIfFileExists;
	functionSet.OpenFile = FatVfsInterfaceOpenFile;
	functionSet.OpenOrCreateFile = FatVfsInterfaceOpenOrCreateFile;
	functionSet.CloseFile = FatVfsInterfaceCloseFile;
	functionSet.ReadFile = FatVfsInterfaceReadFile;
	functionSet.WriteFile = FatVfsInterfaceWriteFile;
	functionSet.AppendFile = FatVfsInterfaceAppendFile;
	functionSet.CreateFile = FatVfsInterfaceCreateFile;
	functionSet.RecreateDeletedFile = FatVfsInterfaceRecreateDeletedFile;
	functionSet.ResizeFile = FatVfsInterfaceResizeFile;
	functionSet.DeleteFile = FatVfsInterfaceDeleteFile;
	functionSet.DeleteAndCloseFile = FatVfsInterfaceDeleteAndCloseFile;
	functionSet.CreateDirectory = FatVfsInterfaceCreateDirectory;
	return functionSet;
}

FAT_DIRECTORY_ENTRY FatEntryFromPath(const char * path)
{
	FAT_DIRECTORY_ENTRY result = { 0 };
	GetFileNameAndExtensionFromPath(path, result.Filename, result.FileExtension);
	return result;
}

UINT32 FatFollowClusterChainToAPoint(BPB* bpb, VFS* vfs, UINT32 start, UINT32 endIndex)
{
	UINT32 numberOfClustersScanned = 0;
	while (FatIsClusterEof(bpb, start) == FALSE && numberOfClustersScanned < endIndex)
	{
		start = FatFollowClusterChain(bpb, vfs, start);
		numberOfClustersScanned++;
	}

	return start;
}

UINT32 FatFollowClusterChainToEnd(BPB* bpb, VFS* vfs, UINT32 start)
{
	UINT32 last = 0;
	while (FatIsClusterEof(bpb, start) == FALSE)
	{
		last = start;
		start = FatFollowClusterChain(bpb, vfs, start);
	}

	return last;
}

VFS_STATUS FatDecreaseClusterChainLengthTo(BPB* bpb, VFS* vfs, UINT32 start, UINT32 removeFrom)
{
	VFS_STATUS status = 0;
	UINT32 clusterChainEnd = FatFollowClusterChainToAPoint(bpb, vfs, start, removeFrom - (removeFrom != 0));
	UINT32 clusterToBeRemoved = (removeFrom != 0) ? FatFollowClusterChain(bpb, vfs, clusterChainEnd) : clusterChainEnd;

	if (removeFrom)
	{
		if (status = FatWriteFatEntry(bpb, vfs, clusterChainEnd, 0, 0, FatGetClusterEof(bpb))) //mark the last cluster as EOF
			return status;
	}
	while (FatIsClusterEof(bpb, clusterToBeRemoved) == false)
	{
		UINT32 currentCluster = clusterToBeRemoved;
		clusterToBeRemoved = FatFollowClusterChain(bpb, vfs, currentCluster); //store the next cluster
		if (status = FatWriteFatEntry(bpb, vfs, currentCluster, 0, 0, 0)) //mark the cluster as empty
			return status;
	}
	return 0;
}

VFS_STATUS FatIncreaseClusterChainLengthBy(BPB* bpb, VFS* vfs, UINT32 start, UINT32 n)
{
	VFS_STATUS status = 0;
	UINT32 cluster = 0, lastCluster = FatFollowClusterChainToEnd(bpb, vfs, start);
	char empty[512] = { 0 };
	UINT32 i;

	for (i = 0; i < n; i++)
	{
		UINT32 j;
		cluster = FatFindFreeCluster(bpb, vfs);

		if (status = FatWriteFatEntry(bpb, vfs, cluster, 0, 0, FatGetClusterEof(bpb)))
			return status;
		if (status = FatWriteFatEntry(bpb, vfs, lastCluster, 0, 0, cluster))
			return status;
		for (j = 0; j < bpb->SectorsPerCluster; j++)
			FatWriteSectorOfCluster(bpb, vfs, cluster, j, empty);
		lastCluster = cluster;
	}

	return 0;
}

VFS_STATUS FatResizeFile(BPB* bpb, VFS* filesystem, FAT_DIRECTORY_ENTRY* parentFile, const char * filename, SIZE_T newSize)
{
	FAT_DIRECTORY_ENTRY file, oldfile;

	VFS_STATUS status = FatReccursivlyFindDirectoryEntry(bpb, filesystem, FatGetFirstClusterOfFile(bpb, parentFile), filename, &file);

	if (status)
		return status;

	oldfile = file;

	UINT32 cluster = FatGetFirstClusterOfFile(bpb, &file);
	UINT32 oldSize = file.FileSize, clusterSize = (bpb->BytesPerSector * bpb->SectorsPerCluster);
	UINT32 oldSizeCluster = (file.FileSize + clusterSize - 1) / clusterSize;
	UINT32 newSizeCluster = ((UINT32)newSize + clusterSize - 1) / clusterSize;

	INT64 changeInClusters = (INT64) newSizeCluster - (INT64) oldSizeCluster;
	UINT32 firstCluster = cluster;

	if (FatIsClusterEof(bpb, cluster))
		return VFS_ERR_EOF;

	if (changeInClusters > 0)
	{
		if (oldSizeCluster == 0 && !firstCluster)
		{
			firstCluster = FatFindFreeCluster(bpb, filesystem);
			if (status = FatWriteFatEntry(bpb, filesystem, firstCluster, 0, 0, FatGetClusterEof(bpb)))
				return status;
			file.LowCluster = firstCluster & 0xFFFF;
			file.HighCluster = (firstCluster & 0xFFFF0000) >> 16;
			changeInClusters--;
		}
		if (changeInClusters > 0)
		{
			if (status = FatIncreaseClusterChainLengthBy(bpb, filesystem, firstCluster, (UINT32)changeInClusters))
				return status;
		}
	}
	else if (changeInClusters < 0)
	{
		if (status = FatDecreaseClusterChainLengthTo(bpb, filesystem, firstCluster, newSizeCluster))
			return status;

		if (newSizeCluster == 0)
		{
			if (status = FatWriteFatEntry(bpb, filesystem, firstCluster, 0, 0, 0))
				return status;

			file.HighCluster = 0;
			file.LowCluster = 0;
		}
	}

	file.FileSize = (UINT32)newSize;

	return FatChangeDirectoryEntry(bpb, filesystem, parentFile, &oldfile, &file);
}

VFS_STATUS FatDeleteFileEntry(BPB* bpb, VFS* vfs, FAT_DIRECTORY_ENTRY* parentDirectory, const char * filename)
{
	VFS_STATUS status;
	FAT_DIRECTORY_ENTRY fatEntry, changedEntry;

	if (status = FatReccursivlyFindDirectoryEntry(bpb, vfs, FatGetFirstClusterOfFile(bpb, parentDirectory), filename, &fatEntry))
		return status;

	changedEntry = fatEntry;
	changedEntry.Filename[0] = FAT_FILE_DELETED;

	if (status = FatChangeDirectoryEntry(bpb, vfs, parentDirectory, &fatEntry, &changedEntry))
		return status;

	return 0;
}

VFS_STATUS FatDeleteDirectoryEntry(BPB* bpb, VFS* vfs, FAT_DIRECTORY_ENTRY* parentDirectory, FAT_DIRECTORY_ENTRY* fatEntry)
{
	VFS_STATUS status;
	FAT_DIRECTORY_ENTRY empty = { 0 };
	char filename[14] = { 0 };
	SIZE_T filenameIndex = 0;
	SIZE_T filenameLength = 0;
	SIZE_T extensionLength = 0;
	SIZE_T i;

	filenameLength = FindCharacterFirst(fatEntry->Filename, 8, ' ');
	extensionLength = FindCharacterFirst(fatEntry->FileExtension, 3, ' ');

	if (filenameLength == -1)
		filenameLength = 8;

	if (extensionLength == -1)
		extensionLength = 3;

	for (i = 0; i < sizeof(empty); i++)
	{
		((char*) (&empty))[i] = 0;
	}

	((char*) &empty)[0] = FAT_FILE_DELETED;

	if (fatEntry->FileAttributes & FAT_DIRECTORY)
	{
		FAT_DIRECTORY_ENTRY previous = { "..      ", "   " };
		FAT_DIRECTORY_ENTRY self = { ".       ", "   " };

		UINT32 currentCluster = FatGetFirstClusterOfFile(bpb, fatEntry);
		BYTE* sectorData = NNXAllocatorAlloc(bpb->BytesPerSector);

		FatChangeDirectoryEntry(bpb, vfs, fatEntry, &previous, &empty);
		FatChangeDirectoryEntry(bpb, vfs, fatEntry, &self, &empty);

		while (currentCluster > 0 && !FatIsClusterEof(bpb, currentCluster))
		{
			UINT32 sectorIndex = 0;
			for (sectorIndex = 0; sectorIndex < bpb->SectorsPerCluster; sectorIndex++)
			{
				SIZE_T currentEntry = 0;
				FatReadSectorOfCluster(bpb, vfs, currentCluster, sectorIndex, sectorData);
				for (currentEntry = 0; currentEntry < bpb->BytesPerSector / 32; currentEntry++)
				{
					FAT_DIRECTORY_ENTRY* current = ((FAT_DIRECTORY_ENTRY*) sectorData) + currentEntry;
					if (current->Filename[0] != 0x00 && current->Filename[0] != FAT_FILE_DELETED)
					{
						FatDeleteDirectoryEntry(bpb, vfs, fatEntry, current);
					}
				}
			}
			currentCluster = FatFollowClusterChain(bpb, vfs, currentCluster);
		}

		NNXAllocatorFree(sectorData);
	}

	for (i = 0; i < filenameLength; i++)
	{
		filename[filenameIndex++] = fatEntry->Filename[i];
	}

	if (extensionLength)
		filename[filenameIndex++] = '.';

	for (i = 0; i < extensionLength; i++)
	{
		filename[filenameIndex++] = fatEntry->FileExtension[i];
	}

	filename[filenameIndex] = 0;

	if (status = FatResizeFile(bpb, vfs, parentDirectory, filename, 0))
		return status;

	if (status = FatReccursivlyFindDirectoryEntry(bpb, vfs, FatGetFirstClusterOfFile(bpb, parentDirectory), filename, fatEntry))
		return status;

	if (status = FatChangeDirectoryEntry(bpb, vfs, parentDirectory, fatEntry, &empty))
	{
		PrintT("[%s:%i] 0x%X\n", __FUNCTION__, __LINE__, status);
		return status;
	}

	return 0;
}

VFS_STATUS FatDeleteFile(BPB* bpb, VFS* vfs, FAT_DIRECTORY_ENTRY* parentDirectory, const char * filename)
{
	VFS_STATUS status;
	FAT_DIRECTORY_ENTRY fatEntry;

	if (status = FatReccursivlyFindDirectoryEntry(bpb, vfs, FatGetFirstClusterOfFile(bpb, parentDirectory), filename, &fatEntry))
		return status;

	if (status = FatDeleteDirectoryEntry(bpb, vfs, parentDirectory, &fatEntry))
		return status;

	return 0;
}

char LogFilepath[] = "efi\\LOG.TXT";
char SecondFilePath[] = "efi\\TEST.TXT";
char RootDirPath[] = "ROOT.DIR";

char Log1[] = "123456789";
char Log2[] = "ABCDEFGHIJKLMNOPQ";

BOOL NNXFatAutomaticTest(VFS* filesystem)
{
#ifdef _DEBUG
	//TODO: FAT16/FAT12 TESTS AND SUPPORT FOR FILESYSTEM OPERATIONS ON THE ROOT DIRECTORY
	UINT32 i = 0;
	PrintT("FAT Test for filesystem 0x%X\n", filesystem);

	for (i = 0; i < 2; i++)
	{
		VFS_STATUS status;

		if (!FatVfsInterfaceCheckIfFileExists(filesystem, LogFilepath))
		{
			if (status = FatVfsInterfaceCreateFile(filesystem, LogFilepath))
			{
				PrintT("[%s] File creation failed.\n", __FUNCTION__);
				return FALSE;
			}
		}

		VFS_FILE* file = FatVfsInterfaceOpenFile(filesystem, LogFilepath);

		if (status = FatVfsInterfaceAppendFile(file, sizeof(Log1) - 1, Log1))
		{
			PrintT("[%s] Append file 1 failed\n", __FUNCTION__);
			return FALSE;
		}

		if (status = FatVfsInterfaceAppendFile(file, sizeof(Log2) - 1, Log2))
		{
			PrintT("[%s] Append file 2 failed\n", __FUNCTION__);
			return FALSE;
		}

		FatVfsInterfaceCloseFile(file);

		if (FatVfsInterfaceCheckIfFileExists(filesystem, SecondFilePath))
		{
			VFS_FILE* file = FatVfsInterfaceOpenFile(filesystem, SecondFilePath);

			if (file)
			{
				FatVfsInterfaceDeleteAndCloseFile(file);
			}
			else
			{
				PrintT("Cannot open file for deletion\n");
				return FALSE;
			}
		}

		if (status = FatVfsInterfaceCreateFile(filesystem, SecondFilePath))
		{
			PrintT("Cannot create file %x\n", status);
			return FALSE;
		}

		if (FatVfsInterfaceCheckIfFileExists(filesystem, "efi\\DIRECTOR.Y"))
		{
			VFS_FILE* dir = FatVfsInterfaceOpenFile(filesystem, "efi\\DIRECTOR.Y");
			FatVfsInterfaceDeleteAndCloseFile(dir);
		}

		if (status = FatVfsInterfaceCreateDirectory(filesystem, "efi\\DIRECTOR.Y"))
		{
			PrintT("Cannot create the directory. 0x%X\n", status);
			return FALSE;
		}

		if (FatVfsInterfaceCheckIfFileExists(filesystem, RootDirPath))
		{
			char readBuffer[9];
			readBuffer[8] = 0;
			VFS_FILE* rootDir = FatVfsInterfaceOpenFile(filesystem, RootDirPath);
			rootDir->FilePointer = 0;
			if (!(status = FatVfsInterfaceWriteFile(rootDir, sizeof(Log1), Log1)))
			{
				if (!(status = FatVfsInterfaceReadFile(rootDir, 8, readBuffer)))
				{
					PrintT("Read: %S\n", readBuffer, 8);

					if (FatVfsInterfaceResizeFile(rootDir, 65536))
						PrintT("Resize 1 failed\n");

					if (FatVfsInterfaceResizeFile(rootDir, 65536 / 2))
						PrintT("Resize 2 failed\n");
				}
				else
				{
					PrintT("Root read failed %x\n", status);
				}
			}
			else
			{
				PrintT("Root write failed %x\n", status);
			}

			FatVfsInterfaceCloseFile(rootDir);
		}
		else
		{
			PrintT("%s does not exist\n", RootDirPath);
		}

		if (FatVfsInterfaceCheckIfFileExists(filesystem, "DELETE.TST"))
		{
			if (status = FatVfsInterfaceDeleteFileFromPath(filesystem, "DELETE.TST"))
			{
				PrintT("Failed to delete DELETE.TST %x\n", status);
			}
			else
			{
				PrintT("Deleted DELETE.TST\n");
			}
		}
		else
		{
			PrintT("DELETE.TST does not exist\n");
		}

		if (!(status = FatVfsInterfaceCreateFile(filesystem, "DELETE.TST")))
		{
			VFS_FILE* file;
			if (file = FatVfsInterfaceOpenFile(filesystem, "DELETE.TST"))
			{
				UINT64 i;
				for (i = 0; i < 8; i++)
					FatVfsInterfaceAppendFile(file, 12, "DELETE.TST\xd\xa");
				FatVfsInterfaceCloseFile(file);
			}
			else
			{
				PrintT("File doesn't exist despite being created\n");
				return FALSE;
			}
		}

		if (FatVfsInterfaceCheckIfFileExists(filesystem, "DONTHAVE.ME"))
		{
			PrintT("File, which the filesystem test relies on not having found.\nTest failed - do you have DONTHAVE.ME in your root directory? If so, delete it, otherwise there's a bug\n");
			return FALSE;
		}

		if (file = FatVfsInterfaceOpenFile(filesystem, "DONTHAVE.ME"))
		{
			PrintT("Opened an (supposedly) non-existend file - test failed\n");
			FatVfsInterfaceCloseFile(file);
			return FALSE;
		}

		if (file = FatVfsInterfaceOpenFile(filesystem, "OBVIOUSIMPOSSIBLEFILENAME"))
		{
			PrintT("Impossible filename opened - test failed\n");
			FatVfsInterfaceCloseFile(file);
			return FALSE;
		}

		if (file = FatVfsInterfaceOpenFile(filesystem, "NINE_____.3__"))
		{
			PrintT("Impossible filename opened - test failed\n");
			FatVfsInterfaceCloseFile(file);
			return FALSE;
		}
	}
	PrintT("[%s] Test success\n", __FUNCTION__);
#endif
	return TRUE;
}