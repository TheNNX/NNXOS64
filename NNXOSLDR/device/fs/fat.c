#include "vfs.h"
#include "../../memory/nnxalloc.h"
#include "../../memory/MemoryOperations.h"
#include "fat.h"
#include "fat32.h"
#include "../../nnxosdbg.h"

UINT64 FindCharacterFirst(char* string, UINT64 len, char character);

UINT32 FATVolumeTotalSize(BPB* bpb) {
	UINT32 volumeTotalSize = bpb->sectorTotSize16;
	if (volumeTotalSize == 0)
		volumeTotalSize = bpb->sectorTotSize32;
	return volumeTotalSize;
}

UINT32 FATFileAllocationTableSize(BPB* bpb) {
	UINT32 fatSize = bpb->sectorFATSize16;
	if (fatSize == 0)
		fatSize = ((BPB32*)&(bpb->_))->sectorFATSize32;

	return fatSize;
}

UINT32 FATGetClusterEOF(BPB* bpb) {
	if (FATisFAT32(bpb))
		return FAT32_RESERVED_CLUSTER_START;
	if (FATisFAT16(bpb))
		return FAT16_RESERVED_CLUSTER_START;
	return FAT12_RESERVED_CLUSTER_START;
}

BOOL FATIsClusterEOF(BPB* bpb, UINT32 cluster) {
	UINT32 maxCluster = FATGetClusterEOF(bpb);
	return cluster >= maxCluster;
}

UINT32 FATCalculateFirstClusterPosition(BPB* bpb) {
	UINT32 rootDirSectors = ((bpb->rootEntryCount * 32) + (bpb->bytesPerSector - 1)) / bpb->bytesPerSector;
	UINT32 fatSize = FATFileAllocationTableSize(bpb); 
	return bpb->sectorReservedSize + bpb->numberOfFATs * fatSize + rootDirSectors;
}

UINT32 FATCalculateFATClusterCount(BPB* bpb) {
	UINT32 volumeTotalSize = FATVolumeTotalSize(bpb);
	UINT32 dataSectorSize = volumeTotalSize - FATCalculateFirstClusterPosition(bpb);
	
	return dataSectorSize / bpb->sectorsPerCluster;
}

BOOL FATisFAT32(BPB* bpb) {
	return bpb->rootEntryCount == 0;
}

BOOL FATisFAT16(BPB* bpb) {
	if (FATisFAT32(bpb))
		return FALSE;

	return FATCalculateFATClusterCount(bpb, 0) > 4084;
}

BOOL FATisFAT12(BPB* bpb) {
	return !(FATisFAT16(bpb) || FATisFAT32(bpb));
}

UINT32 FATLocateMainFAT(BPB* bpb) {
	return bpb->sectorReservedSize;
}

UINT32 FATGetRootCluster(BPB* bpb){
	return FATisFAT32(bpb) ? (((BPB_EXT_FAT32*)&(bpb->_))->firstAccessibleCluster) : 0xFFFFFFFF;
}

UINT32 FATLocateNthFAT(BPB* bpb, UINT32 n) {
	return (bpb->numberOfFATs > n) ? (bpb->sectorReservedSize + FATFileAllocationTableSize(bpb) * n) : 0;
}

VOID FAT32WriteFATEntry(BPB* bpb, VFS* vfs, UINT32 n, BYTE* sectorData, UINT32 value) {
	UINT32 entriesPerSector = bpb->bytesPerSector / 4;
	((UINT32*)sectorData)[n % entriesPerSector] = value;
}

VOID FAT16WriteFATEntry(BPB* bpb, VFS* vfs, UINT32 n, BYTE* sectorData, UINT16 value) {
	UINT32 entriesPerSector = bpb->bytesPerSector / 2;
	((UINT16*)sectorData)[n % entriesPerSector] = value;
}

/**

TODO #1: Check if it actually works.

**/
VOID FAT12WriteFATEntry(BPB* bpb, VFS* vfs, UINT32 n, BYTE* sectorsData, UINT16 value) {
	UINT32 byteOffset = (n * 3) % bpb->bytesPerSector;
	sectorsData += byteOffset;
	*((UINT16*)sectorsData) &= ~0xfff;
	*((UINT16*)sectorsData) |= value;
}


UINT64 FATWriteFATEntryInternal(BPB* bpb, VFS* filesystem, UINT32 n, BYTE* sectorsData, UINT32* currentSector, UINT32 entry) {
	UINT64 status;
	
	FATReadFATEntry(bpb, filesystem, n, sectorsData, currentSector);
	UINT32 mainFAT = FATLocateMainFAT(bpb);
	if (FATisFAT12(bpb)) {
		FAT12WriteFATEntry(bpb, filesystem, n, sectorsData, (UINT16)(entry & 0xfff));
		if (status = VFSWriteSector(filesystem, (*currentSector) + 1, sectorsData + bpb->bytesPerSector))
			return VFS_ERR_READONLY;
	}
	else if (FATisFAT32(bpb)) {
		FAT32WriteFATEntry(bpb, filesystem, n, sectorsData, entry);
	}
	else {
		FAT16WriteFATEntry(bpb, filesystem, n, sectorsData, (UINT16)entry);
	}

	if (status = VFSWriteSector(filesystem, *currentSector, sectorsData))
		return VFS_ERR_READONLY;
}

UINT64 FATWriteFATEntry(BPB* bpb, VFS* filesystem, UINT32 n, BYTE* sectorsData, UINT32* currentSector, UINT32 entry) {
	BOOL manualAllocation = FALSE;
	UINT32 dummy = 0;
	if (currentSector == 0 || sectorsData == 0) {
		manualAllocation = TRUE;
		currentSector = &dummy;
		sectorsData = NNXAllocatorAlloc(bpb->bytesPerSector * 2);
	}
	UINT64 result = FATWriteFATEntryInternal(bpb, filesystem, n, sectorsData, currentSector, entry);

	if (manualAllocation) {
		NNXAllocatorFree(sectorsData);
	}

	return result;
}

UINT32 FAT12ReadFATEntry(BPB* bpb, VFS* filesystem, UINT32 mainFAT, BYTE* sectorsData, UINT32* currentSector, UINT32 n) {
	UINT16 bytesContainingTheEntry;
	UINT32 entryBitOffset = n * 12;
	UINT32 entryByteOffset = entryBitOffset / 8;

	UINT32 relativeLowByteOffset = entryByteOffset % bpb->bytesPerSector;
	UINT32 desiredLowSector = entryByteOffset / bpb->bytesPerSector + mainFAT;

	if (*currentSector != desiredLowSector) {
		UINT64 status = 0;
		status |= VFSReadSector(filesystem, desiredLowSector, sectorsData);
		status |= VFSReadSector(filesystem, desiredLowSector + 1, sectorsData + bpb->bytesPerSector);

		if (status) {
			return 0;
		}

		*currentSector = desiredLowSector;
	}

	bytesContainingTheEntry = *((UINT16*)(sectorsData + relativeLowByteOffset));

	if (entryBitOffset % 8) {
		bytesContainingTheEntry = bytesContainingTheEntry << 4;
	}

	return bytesContainingTheEntry & 0xfff0;
}

UINT32 FAT16or32ReadFATEntry(BPB* bpb, VFS* filesystem, UINT32 mainFAT, BYTE* sectorsData, UINT32* currentSector, UINT32 n) {
	UINT32 entryByteOffset = n * 2 * (FATisFAT32(bpb) + 1);
	UINT32 relativeByteOffset = entryByteOffset % bpb->bytesPerSector;
	UINT32 desiredLowSector = entryByteOffset / bpb->bytesPerSector + mainFAT;

	if (*currentSector != desiredLowSector) {
		UINT64 status;
		if (status = VFSReadSector(filesystem, desiredLowSector, sectorsData)) {
			return 0;
		}

		*currentSector = desiredLowSector;
	}

	if (FATisFAT16(bpb))
		return (UINT16)(*((UINT16*)(sectorsData + relativeByteOffset)));
	else
		return (UINT32)(*((UINT32*)(sectorsData + relativeByteOffset)));
}

UINT32 FATReadFATEntry(BPB* bpb, VFS* filesystem, UINT32 n, BYTE* sectorsData, UINT32* currentSector) {
	UINT32 mainFAT = FATLocateMainFAT(bpb);
	UINT32 entry;
	int altCurSec = 0;
	if (currentSector == 0 || sectorsData == 0) {
		currentSector = &altCurSec;
		sectorsData = NNXAllocatorAlloc(bpb->bytesPerSector * 2);
	}

	if (FATisFAT32(bpb) || FATisFAT16(bpb)) {
		entry = FAT16or32ReadFATEntry(bpb, filesystem, mainFAT, sectorsData, currentSector, n);
	}
	else {	
		entry = FAT12ReadFATEntry(bpb, filesystem, mainFAT, sectorsData, currentSector, n);
	}

	if (entry == 0)
	{
		if (currentSector == &altCurSec) {
			NNXAllocatorFree(sectorsData);
		}
		return 0;
	}

	if (currentSector == &altCurSec) {
		NNXAllocatorFree(sectorsData);
	}

	return entry;
}

BOOL FATIsFree(UINT32 n, BPB* bpb, VFS* filesystem, BYTE* sectorsData, UINT32* currentSector) {
	return !FATReadFATEntry(bpb, filesystem, n, sectorsData, currentSector);
}

UINT32 FATFollowClusterChain(BPB* bpb, VFS* filesystem, UINT32 cluster) {
	return FATReadFATEntry(bpb, filesystem, cluster, 0, 0);
}

/**
	WARNING: not recommended for cluster sizes > 4KiB (NNXAllocator cannot allocate memory above 4KB page size)
	Recommended way of reading clusters is to read each sector individually using FATReadSectorOfCluster
**/
UINT64 FATReadCluster(BPB* bpb, VFS* filesystem, UINT32 clusterIndex, BYTE* data) {
	UINT32 firstSectorOfCluster = bpb->sectorsPerCluster * clusterIndex + FATCalculateFirstClusterPosition(bpb);
	for (int sectorIndex = 0; sectorIndex < bpb->sectorsPerCluster; sectorIndex++) {
		UINT64 status = VFSReadSector(filesystem, firstSectorOfCluster + sectorIndex, data);
		if (status) {
			return status | (sectorIndex << 32);
		}
		data += bpb->bytesPerSector;
	}
	return 0;
}

UINT64 FATReadSectorOfCluster(BPB* bpb, VFS* filesystem, UINT32 clusterIndex, UINT32 sectorIndex, BYTE* data) {
	UINT32 firstSectorOfCluster = bpb->sectorsPerCluster * (clusterIndex - 2) + FATCalculateFirstClusterPosition(bpb);
	
	UINT64 status = VFSReadSector(filesystem, firstSectorOfCluster + sectorIndex, data);
	if (status) {
		return status;
	}

	return 0;
}

UINT64 FATWriteSectorOfCluster(BPB* bpb, VFS* filesystem, UINT32 clusterIndex, UINT32 sectorIndex, BYTE* data) {
	UINT32 firstSectorOfCluster = bpb->sectorsPerCluster * (clusterIndex - 2) + FATCalculateFirstClusterPosition(bpb);

	UINT64 status = VFSWriteSector(filesystem, firstSectorOfCluster + sectorIndex, data);
	if (status) {
		return status;
	}

	return 0;
}

BOOL FATParseDir(FATDirectoryEntry* sectorData, BPB* bpb) {
	for (UINT64 entryIndex = 0; entryIndex < (bpb->bytesPerSector / 32); entryIndex++) {
		if (sectorData[entryIndex].filename[0] == 0x0)
			return 0;
		if (!FATisFileOrDir(sectorData))
			continue;

		PrintT("%i    %s: %S %S\n", entryIndex, ((sectorData[entryIndex].fileAttributes & FAT_DIRECTORY) ? ("Directory") : ("File")), sectorData[entryIndex].filename, (UINT64)8, sectorData[entryIndex].fileExtension, 3);
	}

	return 1;
}

char Uppercase(char a) {
	if (a >= 'a' && a <= 'z')
		return a - 'a' + 'A';
	return a;
}

void FATCopyNameFromEntry(FATDirectoryEntry* entry, char* dst, int* endName, int* endExt) {
	int endOfEntryName = 0;

	*endName = 8;
	for (; (*endName) > 0; (*endName)--) {
		if (entry->filename[(*endName) - 1] != ' ')
			break;
	}

	*endExt = 3;
	for (; (*endExt) > 0; (*endExt)--) {
		if (entry->fileExtension[(*endExt) - 1] != ' ')
			break;
	}

	for (int index = 0; index < (*endName); index++) {
		dst[index] = entry->filename[index];
	}
}

BOOL FATCompareName(FATDirectoryEntry* entry, char* filename) {
	
	char entryName[13] = { 0 };
	
	int endExt, endName;
	FATCopyNameFromEntry(entry, entryName, &endName, &endExt);

	if (endExt != 0) {
		entryName[endName] = '.';
		for (int index = 0; index < endExt; index++) {
			entryName[endName + 1 + index] = entry->fileExtension[index];
		}
	}

	int i = 0;
	while (filename[i]) {
		if (Uppercase(filename[i]) != Uppercase(entryName[i]))
			return false;
		i++;
	}

	return true;
}

BOOL FATisFileOrDir(FATDirectoryEntry* sectorData) {
	if (sectorData->filename[0] == 0x0) {
		return false;
	}
	else if (sectorData->filename[0] == 0xe5) {
		return false;
	}
	else if (sectorData->fileAttributes & FAT_VOLUME_ID) {
		return false;
	}
	return true;
}

UINT64 FATSearchForFileInDirectory(FATDirectoryEntry* sectorData, BPB* bpb, VFS* filesystem, char* name, FATDirectoryEntry* output) {
	for (UINT64 entryIndex = 0; entryIndex < (bpb->bytesPerSector / 32); entryIndex++) {
		if (sectorData[entryIndex].filename[0] == 0x0) {
			return VFS_ERR_EOF;
		}
		if (!FATisFileOrDir(sectorData + entryIndex)) {
			continue;
		}
		else if (FATCompareName(sectorData + entryIndex, name)) {
			*output = sectorData[entryIndex];
			return 0;
		}
	}

	return VFS_ERR_FILE_NOT_FOUND; //scan next entry
}


UINT32 FindFirstSlash(char* path) {
	UINT32 index = 0;
	
	while (*path) {
		if (*path == '\\' || *path == '/')
			return index;
		path++;
		index++;
	}

	return 0;
}

UINT32 FATGetFirstClusterOfFile(BPB* bpb, FATDirectoryEntry* dirEntry) {
	return dirEntry->lowCluster | (FATisFAT32(bpb) ? (dirEntry->highCluster << 16) : 0);
}

UINT64 FATCopyFirstFilenameFromPath(char* path, char* filenameCopy) {
	UINT16 slash = FindFirstSlash(path);

	if (slash > 12) {

		return VFS_ERR_INVALID_PATH;
	}

	if (path[slash + 1] == 0)
		slash = 0;

	if (slash != 0) {
		for (UINT16 i = 0; i < slash; i++) {
			filenameCopy[i] = path[i];
		}
		filenameCopy[slash] = 0;
	}
	else {
		UINT32 i = 0;
		while (path[i] && (i < 13) && (path[i] != '\\' && path[i] != '/')) {
			filenameCopy[i] = path[i];
			i++;
		}
		filenameCopy[i] = 0;
	}

	UINT16 endI;
	for (endI = 13; endI > 0; endI--) {
		if (filenameCopy[endI - 1] == '.') {
			endI--;
			break;
		}
	}

	if (endI > 9 || (endI == 0 && slash > 8)) {
		return VFS_ERR_INVALID_FILENAME;
	}

	return 0;
}

BOOL FATWriteDirectoryEntryToSectors(FATDirectoryEntry* sectorData, BPB* bpb, VFS* filesystem, char* filename, FATDirectoryEntry* fileDir) {
	for (UINT64 entryIndex = 0; entryIndex < (bpb->bytesPerSector / 32); entryIndex++) {
		if (sectorData[entryIndex].filename[0] == 0x0 
			|| FATCompareName(sectorData + entryIndex, filename)) {
			sectorData[entryIndex] = *fileDir;
			return TRUE;
		}
	}

	return FALSE;
}

UINT64 FATReccursivlyFindDirectoryEntry(BPB* bpb, VFS* filesystem, UINT32 parentDirectoryCluster, char* path, FATDirectoryEntry* fileDir) {
	while (*path == '\\' || *path == '/') //skip all initial \ or /
		path++;

	UINT16 slash = FindFirstSlash(path);
	char filenameCopy[13];

	UINT64 status = FATCopyFirstFilenameFromPath(path, filenameCopy);
	if (status) {
		
		return status;
	}

	BYTE* sectorData = NNXAllocatorAlloc(bpb->bytesPerSector);

	if (FATisFAT32(bpb) == false && parentDirectoryCluster == 0xFFFFFFFF) { //we need to find first directory entry manually
		UINT32 fatSize = FATFileAllocationTableSize(bpb);
		UINT32 rootDirStart = bpb->sectorReservedSize + bpb->numberOfFATs * fatSize;
		UINT32 rootDirSectors = ((bpb->rootEntryCount * 32) + (bpb->bytesPerSector - 1)) / bpb->bytesPerSector;
		FATDirectoryEntry directory;
		for (int i = 0; i < rootDirSectors; i++) {
			VFSReadSector(filesystem, rootDirStart + i, sectorData);
			
			UINT32 status = FATSearchForFileInDirectory(sectorData, bpb, filesystem, filenameCopy, &directory);
			if (status == VFS_ERR_FILE_NOT_FOUND) {
				continue;
			}
			if (status == VFS_ERR_EOF) {
				rootDirSectors = 0;
				break;
			}

		}
		NNXAllocatorFree(sectorData);
		if (rootDirSectors == 0) {
			return VFS_ERR_FILE_NOT_FOUND;
		}
		
		if ((directory.fileAttributes & FAT_DIRECTORY) == 0) {
			return VFS_ERR_NOT_A_DIRECTORY;	
		}
		return FATReccursivlyFindDirectoryEntry(bpb, filesystem, directory.lowCluster, path + slash + 1, fileDir);
		
	}
	FATDirectoryEntry *dirEntry = NNXAllocatorAlloc(sizeof(FATDirectoryEntry));

	while (!FATIsClusterEOF(bpb, parentDirectoryCluster)) {
		for (UINT32 sectorIndex = 0; sectorIndex < bpb->sectorsPerCluster; sectorIndex++) {
			FATReadSectorOfCluster(bpb, filesystem, parentDirectoryCluster, sectorIndex, sectorData);
			UINT32 status = FATSearchForFileInDirectory(sectorData, bpb, filesystem, filenameCopy, dirEntry);

			if (status == VFS_ERR_EOF) {
				NNXAllocatorFree(sectorData);
				NNXAllocatorFree(dirEntry);
				
				return VFS_ERR_FILE_NOT_FOUND;
			}
			if (status == VFS_ERR_FILE_NOT_FOUND)
				continue;

			if (slash) {
				if ((dirEntry->fileAttributes & FAT_DIRECTORY) == 0) {
					NNXAllocatorFree(dirEntry);
					NNXAllocatorFree(sectorData);
					return VFS_ERR_NOT_A_DIRECTORY;
				}

				UINT32 fisrtClusterOfFile = FATGetFirstClusterOfFile(bpb, dirEntry);

				NNXAllocatorFree(dirEntry);
				NNXAllocatorFree(sectorData);

				return FATReccursivlyFindDirectoryEntry(bpb, filesystem, fisrtClusterOfFile, path + slash + 1, fileDir);
			}
			else {
				*fileDir = *dirEntry;
				NNXAllocatorFree(sectorData);
				NNXAllocatorFree(dirEntry);
				
				return 0;
			}
		}
		parentDirectoryCluster = FATFollowClusterChain(bpb, filesystem, parentDirectoryCluster);
	}
	NNXAllocatorFree(sectorData);
	NNXAllocatorFree(dirEntry);
	
	return VFS_ERR_FILE_NOT_FOUND;
}

UINT32 FATScanFree(VFS* filesystem) {
	BPB _bpb, *bpb = &_bpb;
	VFSReadSector(filesystem, 0, bpb);
	UINT32 fatSize = FATFileAllocationTableSize(bpb);
	bool isFAT16 = FATisFAT16(bpb);
	
	UINT8 *sectorsData = NNXAllocatorAllocVerbose(((bpb->rootEntryCount == 0 || ((bpb->rootEntryCount != 0) && isFAT16)) ? 1 : 2) * bpb->bytesPerSector);
	UINT32 currentSector = 0;
	UINT32 clusterCount = FATCalculateFATClusterCount(bpb);
	UINT32 freeClusters = 0;
	 
	for (int currentEntry = 1; currentEntry < clusterCount + 2; currentEntry++) {
		freeClusters += FATIsFree(currentEntry, bpb, filesystem, sectorsData, &currentSector);
	}

	NNXAllocatorFree(sectorsData);
	return freeClusters;
}

UINT32 FATFindFreeCluster(BPB* bpb, VFS* vfs) {
	UINT32 clusterCount = FATCalculateFATClusterCount(bpb);
	UINT32 currentSector = 0;
	UINT8 *sectorsData = NNXAllocatorAlloc((FATisFAT12(bpb) ? 2 : 1) * bpb->bytesPerSector);

	for (int currentEntry = 1; currentEntry < clusterCount + 2; currentEntry++) {
		if (FATIsFree(currentEntry, bpb, vfs, sectorsData, &currentSector)) {
			NNXAllocatorFree(sectorsData);
			return currentEntry;
		}
	}

	NNXAllocatorFree(sectorsData);
	return -1;
}


UINT64 FATReadSectorFromFile(BPB* bpb, VFS* vfs, FATDirectoryEntry* file, UINT32 offsetSector, PVOID output) {
	if (file == 0) {
		return VFS_ERR_FILE_NOT_FOUND;
	}

	UINT32 curCluster = FATGetFirstClusterOfFile(bpb, file);
	UINT32 clusterNumberPreceding = offsetSector / bpb->sectorsPerCluster;

	while (clusterNumberPreceding) {
		curCluster = FATFollowClusterChain(bpb, vfs, curCluster);
		if (FATIsClusterEOF(bpb, curCluster))
			return VFS_ERR_EOF;
		if (clusterNumberPreceding == 0)
			break;
		clusterNumberPreceding--;
	}
	UINT64 curSector = offsetSector % bpb->sectorsPerCluster;
	UINT8 buffer[4096];
	UINT32 status;

	if (status = FATReadSectorOfCluster(bpb, vfs, curCluster, curSector, buffer)) {
		return status;
	}

	for (UINT32 index = 0; index < bpb->bytesPerSector; index++) {
		((UINT8*)output)[index] = buffer[index];
	}

	return 0;
}

UINT64 FATWriteSectorsToFile(BPB* bpb, VFS* vfs, FATDirectoryEntry* file, UINT32 index, PVOID input, UINT32 sectorsCount) {
	UINT32 curCluster = FATGetFirstClusterOfFile(bpb, file);
	UINT32 firstClusterPosition = FATCalculateFirstClusterPosition(bpb);

	UINT32 currentSector = 0, currentClusterSector = 0;

	sectorsCount += index;

	while (currentSector < sectorsCount) {
		if (currentSector + bpb->sectorsPerCluster > index) {
			for (UINT32 i = 0; i < bpb->sectorsPerCluster && currentSector < sectorsCount; i++) {
				VFSWriteSector(vfs, currentClusterSector + firstClusterPosition + (curCluster - 2)*bpb->sectorsPerCluster, (QWORD)input);
				currentSector++;
				currentClusterSector++;
			}

			if (sectorsCount == currentSector)
				return 0;
		}
		else {
			currentSector += bpb->sectorsPerCluster;
		}

		currentClusterSector = 0;
		curCluster = FATFollowClusterChain(bpb, vfs, curCluster);
		
		if (FATIsClusterEOF(bpb, curCluster)) {
			return VFS_ERR_EOF;
		}
	}

	return 0;

}

UINT64 FATWriteFile(BPB* bpb, VFS* vfs, FATDirectoryEntry* file, UINT32 offset, UINT32 size, PVOID input, UINT32* writtenBytes) {
	UINT32 startSector = offset / bpb->bytesPerSector;
	UINT32 endSector = (offset + size - 1) / bpb->bytesPerSector;
	UINT32 sectorOffset = offset % bpb->bytesPerSector;
	UINT32 lastSectorEndOffset = (offset + size - 1) % bpb->bytesPerSector;
	UINT8 *temporarySectorData = NNXAllocatorAlloc(bpb->bytesPerSector);
	UINT32 i, localWrittenBytes;

	if (file == 0)
		return VFS_ERR_FILE_NOT_FOUND;

	if (writtenBytes == 0)
		writtenBytes = &localWrittenBytes;
	*writtenBytes = 0;

	if (!lastSectorEndOffset)
		lastSectorEndOffset = bpb->bytesPerSector - 1;

	for (i = startSector; i <= endSector; i++) {

		/**
		
		TODO #6: TEST: Make it so the write can be anywhere in the file, not just sector aligned.

		**/

		UINT32 status;
		if (i == endSector || (i == startSector && sectorOffset)) {
			UINT32 lowerBound, upperBound, j;
			FATReadSectorFromFile(bpb, vfs, file, i, temporarySectorData);
			
			lowerBound = (i == startSector) ? sectorOffset : 0;
			upperBound = (i == endSector) ? lastSectorEndOffset + 1: bpb->bytesPerSector;
			for (j = 0; j < upperBound - lowerBound; j++) {
				temporarySectorData[j + lowerBound] = ((UINT8*)input)[i * bpb->bytesPerSector + j];
				(*writtenBytes)++;
			}
			status = FATWriteSectorsToFile(bpb, vfs, file, i, temporarySectorData, 1);
			
		}
		else {
			status = FATWriteSectorsToFile(bpb, vfs, file, i, ((UINT8*)input) + i * bpb->bytesPerSector, 1);
			(*writtenBytes) += 512;
		}
		

		if (status) {
			NNXAllocatorFree(temporarySectorData);
			return status;
		}
	}

	NNXAllocatorFree(temporarySectorData);
	return 0;
}

UINT64 FATReadFile(BPB* bpb, VFS* vfs, FATDirectoryEntry* file, UINT32 offset, UINT32 size, PVOID output, UINT32* readBytes) {
	UINT32 curCluster = FATGetFirstClusterOfFile(bpb, file);
	UINT32 startSector = offset / bpb->bytesPerSector;
	UINT32 endSector = (offset + size) / bpb->bytesPerSector;
	UINT8 buffer[4096];
	UINT32 localReadBytes = 0;

	if (readBytes == 0)
		readBytes = &localReadBytes;

	*readBytes = 0;

	for (UINT32 i = startSector; i <= endSector; i++) {
		UINT32 lowReadLimit = 0, highReadLimit = bpb->bytesPerSector;
		if (i == startSector) {
			lowReadLimit = offset % bpb->bytesPerSector;
		}
		if (i == endSector)
		{
			highReadLimit = (offset + size) % bpb->bytesPerSector;
		}
		UINT32 status;
		if (status = FATReadSectorFromFile(bpb, vfs, file, i, buffer)) {
			return status;
		}
		for (UINT32 j = lowReadLimit; j < highReadLimit; j++) {
			((UINT8*)output)[(*readBytes)++] = buffer[j];
		}
	}

	return 0;
}

UINT64 FATAPIPseudoOpenFile(VFS* filesystem, char* filename, FATDirectoryEntry* theFile, BPB* bpb) {
	VFSReadSector(filesystem, 0, bpb);
	UINT32 rootCluster = FATGetRootCluster(bpb);
	UINT64 status = FATReccursivlyFindDirectoryEntry(bpb, filesystem, rootCluster, filename, theFile);
	return status;
}

BOOL FATAPICheckIfFileExists(VFS* filesystem, char* path) {
	FATDirectoryEntry theFile;
	BPB _bpb, *bpb = &_bpb;
	UINT64 status = FATAPIPseudoOpenFile(filesystem, path, &theFile, bpb);

	return !status;
}

UINT64 FATAPIReadFile(VFS* filesystem, char* path, UINT64 position, UINT64 size, VOID* output) {
	FATDirectoryEntry theFile;
	BPB _bpb, *bpb = &_bpb;
	UINT64 status = FATAPIPseudoOpenFile(filesystem, path, &theFile, bpb);
	if (status)
		return status;
	UINT64 read = 0;
	status = FATReadFile(bpb, filesystem, &theFile, position, size, output, &read);
	if (status)
		return status;
	if (read < size)
		return VFS_ERR_EOF;

	return 0;
}

UINT64 FATAPIWriteFile(VFS* filesystem, char* path, UINT64 position, UINT64 size, VOID* input) {
	FATDirectoryEntry theFile;
	BPB _bpb, *bpb = &_bpb;
	UINT64 status = FATAPIPseudoOpenFile(filesystem, path, &theFile, bpb);
	if (status)
		return status;
	UINT64 written = 0;
	status = FATWriteFile(bpb, filesystem, &theFile, position, size, input, &written);
	if (status)
		return status;
	if (written < size) 
		return VFS_ERR_EOF;
	return 0;
}

UINT64 FATAPIDeleteFile(VFS* filesystem, char* path) {
	BPB _bpb, *bpb = &_bpb;
	UINT64 status;
	FATDirectoryEntry theFile, parentFile;

	if (status = FATAPIPseudoOpenFile(filesystem, path, &theFile, bpb)) {
		return status;
	}
	
	UINT64 pathLength = FindCharacterFirst(path, -1, 0);
	char* parentPath = NNXAllocatorAlloc(pathLength);
	UINT64 index = pathLength;
	while (index) {
		if (path[index - 1] == '\\' || path[index - 1] == '/')
			break;
		index--;
	}
	if (index != 0) {
		UINT64 i, filenameLength;
		for (i = 0; i < index - 1; i++) {
			parentPath[i] = path[i];
			parentPath[i + 1] = 0;
		}

		if (status = FATAPIPseudoOpenFile(filesystem, parentPath, &parentFile, bpb)) {
			NNXAllocatorFree(parentPath);
			return status;
		}

		if (status = FATDeleteFile(bpb, filesystem, &parentFile, path + index)) {
			NNXAllocatorFree(parentPath);
			return status;
		}
	}
	else {

		if (status = FATDeleteFile(bpb, filesystem, 0, path)) {
			NNXAllocatorFree(parentPath);
			return status;
		}
	}

	NNXAllocatorFree(parentPath);
	return 0;
}

BOOL FATCompareEntries(FATDirectoryEntry* entry1, FATDirectoryEntry* entry2) {
	for (UINT32 i = 0; i < 8; i++) {
		if (i < 3) {
			if (entry1->fileExtension[i] != entry2->fileExtension[i])
				return FALSE;
		}
		if (entry1->filename[i] != entry2->filename[i])
			return FALSE;
	}
	return TRUE;
}

UINT64 FATChangeDirectoryEntry(BPB* bpb, VFS* filesystem, FATDirectoryEntry* parent, FATDirectoryEntry* fileEntry, FATDirectoryEntry* desiredFileEntry) {
	char* buffer = NNXAllocatorAlloc(bpb->bytesPerSector);
	UINT64 status = 0;
	UINT32 offset = 0;
	BOOL done = FALSE;

	while (!done && ((status = FATReadFile(bpb, filesystem, parent, offset, bpb->bytesPerSector, buffer, 0)) == 0)) {
		for (FATDirectoryEntry* currentEntry = buffer;
			currentEntry < ((FATDirectoryEntry*)buffer) + (bpb->bytesPerSector) / sizeof(FATDirectoryEntry);
			currentEntry++)
		{
			if (((fileEntry->filename[0] == FAT_FILE_DELETED || fileEntry->filename[0] == 0) && fileEntry->filename[0] == currentEntry->filename[0])
				|| FATCompareEntries(fileEntry, currentEntry))
			{
				*currentEntry = *desiredFileEntry;
				status = FATWriteFile(bpb, filesystem, parent, offset, bpb->bytesPerSector, buffer, 0);
				done = TRUE;
				break;
			}
		}

		offset += bpb->bytesPerSector;
	}

	NNXAllocatorFree(buffer);
	return status;
}

UINT64 FATAddDirectoryEntry(BPB* bpb, VFS* filesystem, FATDirectoryEntry* parent, FATDirectoryEntry* fileEntry) {
	FATDirectoryEntry empty = { 0 };
	return FATChangeDirectoryEntry(bpb, filesystem, parent, &empty, fileEntry);
}

UINT64 FATAPICreateFile(VFS* filesystem, char* path) {
	if (FATAPICheckIfFileExists(filesystem, path))
		return VFS_ERR_FILE_ALREADY_EXISTS;

	BPB _bpb, *bpb = &_bpb;

	FATDirectoryEntry fileEntry = { 0 };
	UINT32 borderPoint = FATGetFileNameAndExtensionFromPath(path, fileEntry.filename, fileEntry.fileExtension);
	UINT32 filenameLength = FindCharacterFirst(fileEntry.filename, -1, 0);
	UINT32 extensionLength = FindCharacterFirst(fileEntry.fileExtension, -1, 0);

	FATDirectoryEntry parent = { 0 };

	if (borderPoint) {
		char *parentPath = NNXAllocatorAlloc(borderPoint);

		for (UINT32 i = 0; i < borderPoint - 1; i++) {
			parentPath[i] = path[i];
		}

		parentPath[borderPoint - 1] = 0;

		FATAPIPseudoOpenFile(filesystem, parentPath, &parent, bpb);

		if (parent.highCluster == 0 && parent.lowCluster == 0) {
			UINT64 status;
			UINT32 cluster = FATFindFreeCluster(bpb, filesystem);

			if (status = FATWriteFATEntry(bpb, filesystem, cluster, 0, 0, FATGetClusterEOF(bpb))) {
				PrintT("Cluster allocation encountered an error\n");
				return status;
			}
		}

		UINT32 parentCluster = FATGetFirstClusterOfFile(bpb, &parent);
		UINT64 status = FATAddDirectoryEntry(bpb, filesystem, &parent, &fileEntry);

		if (status == VFS_ERR_EOF) {
			status = FATAppendTrailingClusters(bpb, filesystem, parentCluster, 1);
			if (status == 0) {
				status = FATAddDirectoryEntry(bpb, filesystem, &parent, &fileEntry);
			}
			else {
				status = VFS_ERR_NOT_ENOUGH_ROOM_FOR_WRITE;
				PrintT("%i: %x\n", __LINE__, status);
			}
		}

		NNXAllocatorFree(parentPath);
		return status;
	}
	else {

		NNXAssertAndStop(0, "Unimplemented");
	}
}

FunctionSet FATGetFunctionSet() {
	FunctionSet functionSet = { 0 };
	functionSet.CheckIfFileExists = FATAPICheckIfFileExists;
	functionSet.ReadFile = FATAPIReadFile;
	functionSet.CreateFile = FATAPICreateFile;
	functionSet.WriteFile = FATAPIWriteFile;

	return functionSet;
}

UINT64 FindCharacterLast(char* string, UINT64 len, char character) {
	NNXAssertAndStop(len != -1, "Cannot find the last character of string with unknown length.");
	
	UINT64 current = -1;

	for (UINT64 i = 0; i < len; i++) {
		if (string[i] == character) {
			current = i;
		}
	}

	return current;
}

/*
len can be 0xFFFFFFFF/-1 if length is undefined, for ex. you can use FindCharacterFirst(string, -1, 0) to measure the length itself 
*/
UINT64 FindCharacterFirst(char* string, UINT64 len, char character) {
	for (UINT64 i = 0; i < len; i++) {
		if (string[i] == character) {
			return i;
		}
	}

	return -1;
}

UINT64 FATGetFileNameAndExtensionFromPath(char* path, char* name, char* extension) {

	for (UINT64 i = 0; i < 8; i++)
		name[i] = ' ';

	for (UINT64 i = 0; i < 3; i++)
		extension[i] = ' ';

	UINT64 length = FindCharacterFirst(path, -1, 0);
	UINT64 begin = FindCharacterLast(path, length, '\\');
	if (begin == -1)
		begin = FindCharacterLast(path, length, '/');
	begin++;
	UINT64 dot = FindCharacterLast(path + begin, length - begin, '.');
	UINT64 filenameEnd = dot == -1 ? length - begin : dot;
	NNXAssertAndStop(filenameEnd <= 8, "Invalid filename");
	for (UINT64 i = 0; i < filenameEnd; i++) {
		name[i] = (path + begin)[i];
	}
	if (dot != -1) {
		NNXAssertAndStop(length - begin - dot - 1 <= 3, "Invalid extension");
		for (UINT64 i = 0; i < length - begin - dot - 1; i++) {
			extension[i] = (path + begin + dot + 1)[i];
		}
	}
	return begin;
}

FATDirectoryEntry FATEntryFromPath(char* path) {
	FATDirectoryEntry result = { 0 };
	FATGetFileNameAndExtensionFromPath(path, result.filename, result.fileExtension);
	return result;
}

UINT32 FATPathParser(char* path, UINT32 currentIndex) {
	path += currentIndex;
	
	UINT32 position = FindFirstSlash(path);
	if (position == 0)
		return -1;

	return position + 1;
}

UINT32 FATFollowClusterChainToAPoint(BPB* bpb, VFS* vfs, UINT32 start, UINT32 endIndex) {
	UINT32 lastCluster = start;
	UINT32 numberOfClustersScanned = 0;
	while (FATIsClusterEOF(bpb, start) == false && numberOfClustersScanned < endIndex) {
		lastCluster = start;
		start = FATFollowClusterChain(bpb, vfs, start);
		numberOfClustersScanned++;
	}

	return lastCluster;
}

UINT32 FATFollowClusterChainToEnd(BPB* bpb, VFS* vfs, UINT32 start) {
	return FATFollowClusterChainToAPoint(bpb, vfs, start, 0xFFFFFFFF);
}


UINT64 FATRemoveTrailingClusters(BPB* bpb, VFS* vfs, UINT32 start, UINT32 removeFrom) {
	UINT64 status;
	UINT32 clusterChainEnd = FATFollowClusterChainToAPoint(bpb, vfs, start, removeFrom); //cluster number of cluster preeceding the first one to be removed
	UINT32 clusterToBeRemoved = FATFollowClusterChain(bpb, vfs, clusterChainEnd);
	
	if (status = FATWriteFATEntry(bpb, vfs, clusterChainEnd, 0, 0, FATGetClusterEOF(bpb))) //mark the last cluster as EOF
		return status;

	while (FATIsClusterEOF(bpb, clusterToBeRemoved) == false) {

		UINT32 clusterChainCurrent = clusterToBeRemoved;
		clusterToBeRemoved = FATFollowClusterChain(bpb, vfs, clusterChainCurrent); //store the next cluster
		if (status = FATWriteFATEntry(bpb, vfs, clusterChainCurrent, 0, 0, 0)) //mark the cluster as empty
			return status;
	}

	return 0;
}

UINT64 FATAppendTrailingClusters(BPB* bpb, VFS* vfs, UINT32 start, UINT32 n) {
	UINT64 status = 0;
	UINT32 lastCluster = FATFollowClusterChainToEnd(bpb, vfs, start);

	for (UINT32 i = 0; i < n; i++) {
		UINT32 cluster = FATFindFreeCluster(bpb, vfs);

		if (status = FATWriteFATEntry(bpb, vfs, cluster, 0, 0, FATGetClusterEOF(bpb)))
			return status;
		if (status = FATWriteFATEntry(bpb, vfs, lastCluster, 0, 0, cluster))
			return status;
		lastCluster = cluster;
	}

	lastCluster = FATFollowClusterChainToEnd(bpb, vfs, start);

	return 0;
}

UINT64 FATResizeFile(BPB* bpb, VFS* filesystem, FATDirectoryEntry* parentFile, char* filename, UINT64 newSize) {
	FATDirectoryEntry file, oldfile;
	UINT64 status = FATReccursivlyFindDirectoryEntry(bpb, filesystem, FATGetFirstClusterOfFile(bpb, parentFile), filename, &file);
	
	if (status)
		return status;

	oldfile = file;

	UINT32 cluster = FATGetFirstClusterOfFile(bpb, &file);
	UINT32 oldSize = file.fileSize, clusterSize = (bpb->bytesPerSector * bpb->sectorsPerCluster);
	UINT32 oldSizeCluster = (file.fileSize + clusterSize - 1) / clusterSize;
	UINT32 newSizeCluster = (newSize + clusterSize - 1) / clusterSize;


	INT64 changeInClusters = ((INT64)newSizeCluster) - ((INT64)oldSizeCluster);
	UINT32 firstCluster = FATGetFirstClusterOfFile(bpb, &file);

	if (changeInClusters > 0) {
		if (oldSizeCluster == 0 && !firstCluster) {
			firstCluster = FATFindFreeCluster(bpb, filesystem);
			if (status = FATWriteFATEntry(bpb, filesystem, firstCluster, 0, 0, FATGetClusterEOF(bpb)))
				return status;
			file.lowCluster = firstCluster & 0xFFFF;
			file.highCluster = (firstCluster & 0xFFFF0000) >> 16;
			changeInClusters--;
		}
		if (changeInClusters > 0) {
			if (status = FATAppendTrailingClusters(bpb, filesystem, firstCluster, changeInClusters))
				return status;
		}
	}
	else if (changeInClusters < 0) {
		if (status = FATRemoveTrailingClusters(bpb, filesystem, firstCluster, newSizeCluster))
			return status;

		if (newSizeCluster == 0) {
			if(status = FATWriteFATEntry(bpb, filesystem, firstCluster, 0, 0, 0))
				return status;
			file.highCluster = 0;
			file.lowCluster = 0;
		}	
	}

	file.fileSize = newSize;

	return FATChangeDirectoryEntry(bpb, filesystem, parentFile, &oldfile, &file);
}

UINT64 FATDeleteFileEntry(BPB* bpb, VFS* vfs, FATDirectoryEntry* parentDirectory, char* filename) {
	UINT64 status;
	FATDirectoryEntry fatEntry, changedEntry;
	if (status = FATReccursivlyFindDirectoryEntry(bpb, vfs, FATGetFirstClusterOfFile(bpb, parentDirectory), filename, &fatEntry))
		return status;
	changedEntry = fatEntry;
	changedEntry.filename[0] = FAT_FILE_DELETED;
	if (status = FATChangeDirectoryEntry(bpb, vfs, parentDirectory, &fatEntry, &changedEntry))
		return status;
	return 0;
}

UINT64 FATDeleteFile(BPB* bpb, VFS* vfs, FATDirectoryEntry* parentDirectory, char* filename) {
	UINT64 status;
	FATDirectoryEntry fatEntry, empty;
	for (UINT32 i = 0; i < sizeof(empty); i++) {
		((char*)(&empty))[i] = 0;
	}

	if (status = FATResizeFile(bpb, vfs, parentDirectory, filename, 0)) 
		return status;
	if (status = FATReccursivlyFindDirectoryEntry(bpb, vfs, FATGetFirstClusterOfFile(bpb, parentDirectory), filename, &fatEntry))
		return status;
	if (status = FATChangeDirectoryEntry(bpb, vfs, parentDirectory, &fatEntry, &empty))
		return status;

	return 0;
}

BOOL Test(char* path, VFS* filesystem, FATDirectoryEntry* file) {
	BPB _bpb, *bpb = &_bpb;
	VFSReadSector(filesystem, 0, bpb);
	UINT32 rootCluster = FATisFAT32(bpb) ? (((BPB_EXT_FAT32*)&(bpb->_))->firstAccessibleCluster) : 0xFFFFFFFF;
	FATDirectoryEntry theFile;

	UINT32 status = 0;
	if (status = FATReccursivlyFindDirectoryEntry(bpb, filesystem, rootCluster, path, &theFile)) {
		return FALSE;
	}
	else {
		if (file)
			*file = theFile;
		
		return TRUE;
	}
}

BOOL TestFile(char* path, VFS* filesystem) {
	PrintT("Test file '%s'\n", path);
	return Test(path, filesystem, 0);
}

BOOL TestWrite(char* path, VFS* filesystem, UINT32 offset) {
	PrintT("Test write '%s'\n", path);
	BPB _bpb, *bpb = &_bpb;
	VFSReadSector(filesystem, 0, bpb);
	FATDirectoryEntry theFile;
	BOOL testSuccess = Test(path, filesystem, &theFile);
	if (!testSuccess)
		return FALSE;

	char inputText[513] = "input text test overrite (hopefully just the right part of the right file, and not the disk)";

	UINT64 status = FATWriteSectorsToFile(bpb, filesystem, &theFile, offset / bpb->bytesPerSector, inputText, 1);

	return status ? 0 : 1;
}

BOOL TestRead(char* path, VFS* filesystem, UINT32 offset, UINT32 size) {
	PrintT("Test read '%s'\n",path);
	BPB _bpb, *bpb = &_bpb;
	VFSReadSector(filesystem, 0, bpb);
	FATDirectoryEntry theFile;
	BOOL testSuccess = Test(path, filesystem, &theFile);
	if (!testSuccess)
		return FALSE;
	UINT32 readBytes = 0;
	UINT8 *outbuffer = NNXAllocatorAlloc(size + 1);
	outbuffer[size] = 0;
	UINT32 status;
	if (status = FATReadFile(bpb, filesystem, &theFile, offset, size, outbuffer, &readBytes)) {
		PrintT("Error reading file %x\n", status);
		return FALSE;
	}
	
	NNXAllocatorFree(outbuffer);
	return TRUE;
}


BOOL NNX_FATAutomaticTest(VFS* filesystem) {
	char filename[] = "efi\\DBGFILE.TXT";
	char toAppend1[] = "append 1\n\n\n";
	char toAppend2[] = "append 2";
	UINT64 status;
	BPB _bpb, *bpb = &_bpb;
	FATDirectoryEntry theFile;
	FATDirectoryEntry parent;

	if (FATAPICheckIfFileExists(filesystem, filename)) {
		if (status = FATAPIDeleteFile(filesystem, filename)){
			PrintT("Could not delete file %s, status: 0x%X\n", filename, status);
			return FALSE;
		}
	}

	if (status = FATAPICreateFile(filesystem, filename)) {
		PrintT("Could not create file %s, status: 0x%X\n", filename, status);
		return FALSE;
	}

	if (status = FATAPIPseudoOpenFile(filesystem, filename, &theFile, bpb)) {
		PrintT("Could not open file %s, status 0x%X\n", filename, status);
		return FALSE;
	}


	UINT32 currentOffset = theFile.fileSize;
	UINT32 newSize = sizeof(toAppend1) + sizeof(toAppend2) + currentOffset - 2;
	FATAPIPseudoOpenFile(filesystem, "efi", &parent, bpb);
	FATResizeFile(bpb, filesystem, &parent, "DBGFILE.TXT", newSize);

	if (status = FATAPIPseudoOpenFile(filesystem, filename, &theFile, bpb)) {
		PrintT("Could not open file %s, status 0x%X\n", filename, status);
		return FALSE;
	}

	FATAPIWriteFile(filesystem, filename, currentOffset, sizeof(toAppend1) - 1, toAppend1);
	currentOffset += sizeof(toAppend1) - 1;
	FATAPIWriteFile(filesystem, filename, currentOffset, sizeof(toAppend2) - 1, toAppend2);
	currentOffset += sizeof(toAppend2) - 1;

	return TRUE;
}