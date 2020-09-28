#include "vfs.h"
#include "../../memory/nnxalloc.h"
#include "../../memory/MemoryOperations.h"
#include "fat.h"
#include "fat32.h"
#include "../../nnxosdbg.h"

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

BOOL FATIsClusterEOF(BPB* bpb, UINT32 cluster) {
	if(FATisFAT32(bpb))
		return cluster >= FAT32_RESERVED_CLUSTER_START;
	if (FATisFAT16(bpb))
		return cluster >= FAT16_RESERVED_CLUSTER_START;
	return cluster >= FAT12_RESERVED_CLUSTER_START;
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

UINT32 FATLocateNthFAT(BPB* bpb, UINT32 n) {
	return (bpb->numberOfFATs > n) ? (bpb->sectorReservedSize + FATFileAllocationTableSize(bpb) * n) : 0;
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
		UINT32 entryByteOffset = n * 2 * (FATisFAT32(bpb) + 1);

		UINT32 relativeByteOffset = entryByteOffset % bpb->bytesPerSector;
		UINT32 desiredLowSector = entryByteOffset / bpb->bytesPerSector + mainFAT;

		if (*currentSector != desiredLowSector) {
			UINT64 status;
			if (status = VFSReadSector(filesystem, desiredLowSector, sectorsData)) {
				if (currentSector == &altCurSec) {
					NNXAllocatorFree(sectorsData);
				}
				return 0;
			}

			*currentSector = desiredLowSector;
		}

		if(FATisFAT16(bpb))
			entry = (UINT16)(*((UINT16*)(sectorsData + relativeByteOffset)));
		else
			entry = (UINT32)(*((UINT32*)(sectorsData + relativeByteOffset)));
	}
	else {	
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
				if (currentSector == &altCurSec) {
					NNXAllocatorFree(sectorsData);
				}
				return 0;
			}

			*currentSector = desiredLowSector;
		}

		bytesContainingTheEntry = *((UINT16*)(sectorsData + relativeLowByteOffset));

		if (entryBitOffset % 8) {
			bytesContainingTheEntry = bytesContainingTheEntry << 4;
		}

		entry = bytesContainingTheEntry & 0xfff0;
	}

	if (currentSector == &altCurSec) {
		NNXAllocatorFree(sectorsData);
	}

	return entry;
}

BOOL FATWriteFATEntryToCacheSectors(BPB* bpb, UINT32 n, UINT32 value, UINT8* buffer, UINT64 bufferSize) {
	if (FATisFAT32(bpb)) {
		if (n * sizeof(UINT64) > bufferSize)
			return false;
		((UINT64*)buffer)[n] = value;
	}
	else if (FATisFAT16(bpb)){
		if (n * sizeof(UINT32) > bufferSize)
			return false;
		((UINT16*)buffer)[n] = (UINT16)(value);
	}
	else {

		UINT64 byteOffset = n * 12 / 8;
		UINT64 relativeByteOffset = byteOffset % bpb->bytesPerSector;

		if (bufferSize <= 512 && relativeByteOffset >= 511) {
			return false;
		}

		if ((n * 12) % 8) {
			*((UINT16*)(buffer + relativeByteOffset)) &= ~(0x0fff);
			*((UINT16*)(buffer + relativeByteOffset)) |= ((value >> 4) & 0x0fff);
		}
		else {
			*((UINT16*)(buffer + relativeByteOffset)) &= ~(0xfff0);
			*((UINT16*)(buffer + relativeByteOffset)) |= ((value) & 0xfff0);
		}
	}

	return true;
}

UINT64 FATCommitSectorToAllFATs(BPB* bpb, VFS* filesystem, UINT32 secNum, UINT8* data) {
	UINT32 fatLocation;
	UINT32 i = 0;
	
	while (fatLocation = FATLocateNthFAT(bpb, i++)) {
		UINT64 status;
		if (status = VFSWriteSector(filesystem, secNum, data))
			return status;
	}

	return 0;
}

BOOL FATIsFree(UINT32 n, BPB* bpb, VFS* filesystem, BYTE* sectorsData, UINT32* currentSector) {
	return !FATReadFATEntry(bpb, filesystem, n, sectorsData, currentSector);
}

UINT32 FATFollowClusterChain(BPB* bpb, VFS* filesystem, UINT32 cluster) {
	return FATReadFATEntry(bpb, filesystem, cluster, 0, 0);
}

/**
	WARNING: not recommended for cluster sizes > 4KB (NNXAllocator cannot allocate memory above 4KB page size)
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


UINT16 FindFirstSlash(char* path) {
	UINT16 index = 0;
	
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

UINT64 FATCopyFilenameFromPath(char* path, char* filenameCopy) {
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

UINT64 FATReccursivlyWriteDirectoryEntry(BPB* bpb, VFS* filesystem, UINT32 parentDirectoryCluster, char* path, FATDirectoryEntry* fileDir) {
	while (*path == '\\' || *path == '/') //skip all initial \ or /
		path++;

	UINT16 slash = FindFirstSlash(path);
	char filenameCopy[13];

	UINT64 status = FATCopyFilenameFromPath(path, filenameCopy);
	if (status)
		return status;

	BYTE* sectorData = NNXAllocatorAlloc(bpb->bytesPerSector);

	if (FATisFAT32(bpb) == false && parentDirectoryCluster == 0xFFFFFFFF) { //we need to find first directory entry manually
		UINT32 fatSize = FATFileAllocationTableSize(bpb);
		UINT32 rootDirStart = bpb->sectorReservedSize + bpb->numberOfFATs * fatSize;
		UINT32 rootDirSectors = ((bpb->rootEntryCount * 32) + (bpb->bytesPerSector - 1)) / bpb->bytesPerSector;
		FATDirectoryEntry directory;
		for (int i = 0; i < rootDirSectors; i++) {
			VFSReadSector(filesystem, rootDirStart + i, sectorData);

			if (FATWriteDirectoryEntryToSectors(sectorData, bpb, filesystem, path, fileDir))
			{
				UINT64 status = VFSWriteSector(filesystem, rootDirStart + i, sectorData);
				if (status) {
					NNXAllocatorFree(sectorData);
					return status;
				}
			}
		}
		NNXAllocatorFree(sectorData);
		if (rootDirSectors == 0) {
			SHOWCODEPOS;
			return VFS_ERR_FILE_NOT_FOUND;
		}

		if ((directory.fileAttributes & FAT_DIRECTORY) == 0) {

			return VFS_ERR_NOT_A_DIRECTORY;
		}
		return FATReccursivlyWriteDirectoryEntry(bpb, filesystem, directory.lowCluster, path + slash + 1, fileDir);

	}
	FATDirectoryEntry *dirEntry = NNXAllocatorAlloc(sizeof(FATDirectoryEntry));

	while (!FATIsClusterEOF(bpb, parentDirectoryCluster)) {
		for (UINT32 sectorIndex = 0; sectorIndex < bpb->sectorsPerCluster; sectorIndex++) {
			FATReadSectorOfCluster(bpb, filesystem, parentDirectoryCluster, sectorIndex, sectorData);
			UINT64 status = FATSearchForFileInDirectory(sectorData, bpb, filesystem, filenameCopy, dirEntry);
			UINT64 status2;

			if (slash && status != VFS_ERR_FILE_NOT_FOUND) {
				if ((dirEntry->fileAttributes & FAT_DIRECTORY) == 0) {
					NNXAllocatorFree(dirEntry);
					NNXAllocatorFree(sectorData);
					return VFS_ERR_NOT_A_DIRECTORY;
				}

				UINT32 fisrtClusterOfFile = FATGetFirstClusterOfFile(bpb, dirEntry);

				NNXAllocatorFree(dirEntry);
				NNXAllocatorFree(sectorData);

				return FATReccursivlyWriteDirectoryEntry(bpb, filesystem, fisrtClusterOfFile, path + slash + 1, fileDir);
			}
			else if(status2 = FATWriteDirectoryEntryToSectors(sectorData, bpb, filesystem, path, fileDir)){
				FATWriteSectorOfCluster(bpb, filesystem, parentDirectoryCluster, sectorIndex, sectorData);
				NNXAllocatorFree(sectorData);
				NNXAllocatorFree(dirEntry);

				return 0;
			}
			else if(status == VFS_ERR_EOF){
				PrintT("%i %X %X\n", slash, status, status2);
				SHOWCODEPOS;
				NNXAllocatorFree(sectorData);
				NNXAllocatorFree(dirEntry);
				return VFS_NOT_ENOUGH_ROOM_FOR_WRITE;
			}
		}
		parentDirectoryCluster = FATFollowClusterChain(bpb, filesystem, parentDirectoryCluster);
	}

	NNXAllocatorFree(sectorData);
	NNXAllocatorFree(dirEntry);
	SHOWCODEPOS;
	return VFS_ERR_FILE_NOT_FOUND;
}

UINT64 FATReccursivlyReadDirectoryEntry(BPB* bpb, VFS* filesystem, UINT32 parentDirectoryCluster, char* path, FATDirectoryEntry* fileDir) {
	while (*path == '\\' || *path == '/') //skip all initial \ or /
		path++;

	UINT16 slash = FindFirstSlash(path);
	char filenameCopy[13];

	UINT64 status = FATCopyFilenameFromPath(path, filenameCopy);
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
		return FATReccursivlyReadDirectoryEntry(bpb, filesystem, directory.lowCluster, path + slash + 1, fileDir);
		
	}
	FATDirectoryEntry *dirEntry = NNXAllocatorAlloc(sizeof(FATDirectoryEntry));

	while (!FATIsClusterEOF(bpb, parentDirectoryCluster)) {
		for (UINT32 sectorIndex = 0; sectorIndex < bpb->sectorsPerCluster; sectorIndex++) {
			FATReadSectorOfCluster(bpb, filesystem, parentDirectoryCluster, sectorIndex, sectorData);
			UINT32 status = FATSearchForFileInDirectory(sectorData, bpb, filesystem, filenameCopy, dirEntry);
			
			PrintT("Test");

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

				return FATReccursivlyReadDirectoryEntry(bpb, filesystem, fisrtClusterOfFile, path + slash + 1, fileDir);
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
	UINT8 *sectorsData = NNXAllocatorAlloc(((bpb->rootEntryCount == 0 || ((bpb->rootEntryCount != 0) && FATisFAT16(bpb))) ? 1 : 2) * bpb->bytesPerSector);

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
	UINT32 clusterStart = index / (bpb->sectorsPerCluster * bpb->bytesPerSector);
	UINT32 curCluster = FATGetFirstClusterOfFile(bpb, file);
	UINT32 firstClusterPosition = FATCalculateFirstClusterPosition(bpb);

	UINT32 currentSector = 0;

	sectorsCount += index;

	while (currentSector < sectorsCount) {
		if (currentSector >= index) {
			for (UINT32 i = 0; i < bpb->sectorsPerCluster && currentSector < sectorsCount; i++) {
				VFSWriteSector(vfs, currentSector % (bpb->sectorsPerCluster) + firstClusterPosition + (curCluster - 2)*bpb->sectorsPerCluster, input);
				currentSector++;
			}

			if (sectorsCount == currentSector)
				return 0;
		}
		else {
			currentSector += bpb->sectorsPerCluster;
		}

		curCluster = FATFollowClusterChain(bpb, vfs, curCluster);
		
		if (FATIsClusterEOF(bpb, curCluster)) {
			return VFS_ERR_EOF;
		}
	}

	return 0;

}

UINT64 FATWriteFile(BPB* bpb, VFS* vfs, FATDirectoryEntry* file, UINT32 offset, UINT32 size, PVOID input, UINT32* writtenBytes) {
	if (file == 0)
		return VFS_ERR_FILE_NOT_FOUND;

	*writtenBytes = 0;

	UINT32 startSector = offset / bpb->bytesPerSector;
	UINT32 endSector = (offset + size) / bpb->bytesPerSector;

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
		if (status = FATWriteSectorsToFile(bpb, vfs, file, i, (((UINT64)input) + bpb->bytesPerSector * i), 1)) {
			return status;
		}
	}

	return 0;
}

UINT64 FATReadFile(BPB* bpb, VFS* vfs, FATDirectoryEntry* file, UINT32 offset, UINT32 size, PVOID output, UINT32* readBytes) {
	UINT32 curCluster = FATGetFirstClusterOfFile(bpb, file);
	UINT32 startSector = offset / bpb->bytesPerSector;
	UINT32 endSector = (offset + size) / bpb->bytesPerSector;
	UINT8 buffer[4096];

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

BOOL Test(char* path, VFS* filesystem, FATDirectoryEntry* file) {
	BPB _bpb, *bpb = &_bpb;
	VFSReadSector(filesystem, 0, bpb);
	UINT32 rootCluster = FATisFAT32(bpb) ? (((BPB_EXT_FAT32*)&(bpb->_))->firstAccessibleCluster) : 0xFFFFFFFF;
	FATDirectoryEntry theFile;

	UINT32 status = 0;
	if (status = FATReccursivlyReadDirectoryEntry(bpb, filesystem, rootCluster, path, &theFile)) {
		return false;
	}
	else {
		if (file)
			*file = theFile;
		
		return true;
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
		return false;

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
		return false;
	UINT32 readBytes = 0;
	UINT8 *outbuffer = NNXAllocatorAlloc(size + 1);
	outbuffer[size] = 0;
	UINT32 status;
	if (status = FATReadFile(bpb, filesystem, &theFile, offset, size, outbuffer, &readBytes)) {
		PrintT("Error reading file %x\n", status);
		return false;
	}
	
	NNXAllocatorFree(outbuffer);
	return true;
}

BOOL TestWrite2(const char* path, VFS* filesystem) {
	BPB _bpb, *bpb = &_bpb;
	VFSReadSector(filesystem, 0, bpb);
	FATDirectoryEntry entry = { 0 };

	for (UINT32 i = 0; i < 8; i++) {
		if (i < 3)
			entry.fileExtension[i] = ("TXT")[i];
		entry.filename[i] = ("README  ")[i];
	}

	UINT32 rootCluster = FATisFAT32(bpb) ? (((BPB_EXT_FAT32*)&(bpb->_))->firstAccessibleCluster) : 0xFFFFFFFF;
	UINT64 status = FATReccursivlyWriteDirectoryEntry(bpb, filesystem, rootCluster, path, &entry);
	PrintT("File creation status 0x%X\n", status);
	return !status;
}

BOOL NNX_FATAutomaticTest(VFS* filesystem) {
	
	bool test[32];
	for (int j = 0; j < sizeof(test) / sizeof(*test); j++)
		test[j] = 2;
	int i = 0;

	test[i++] = TestFile("efi\\boot\\NNXOSCFG.TXT", filesystem);
	test[i++] = !TestFile("efi\\boot3\\NOTADIR.TXT", filesystem);
	test[i++] = !TestFile("efi\\boot\\FNFound.TXT", filesystem);
	test[i++] = !TestFile("efi\\boot\\NNXOSCFG.TXT\\NOTA.DIR", filesystem);
	test[i++] = TestFile("efi\\", filesystem);
	test[i++] = TestFile("efi", filesystem);
	test[i++] = TestRead("efi\\boot\\NNXOSCFG.TXT", filesystem, 0, 10);
	test[i++] = TestRead("efi\\boot\\LRGEHALF.TXT", filesystem, 2048, 16);
	test[i++] = TestRead("efi\\boot\\LRGE.TXT", filesystem, 0x7000, 16);
	test[i++] = TestWrite("efi\\boot\\LRGE.TXT", filesystem, 0x7000);
	test[i++] = TestRead("efi\\boot\\LRGE.TXT", filesystem, 0x7000, 16);
	test[i++] = TestWrite2("efi\\boot\\README.TXT", filesystem);

	for (int j = 0; j < sizeof(test) / sizeof(*test) && test[j] != 2; j++) 
		PrintT("%s%s", test[j] ? ("True") : ("False"), (sizeof(test) / sizeof(*test) == j + 1 || (test[j+1] == 2)) ? ("\n") : (", "));
	

	for (int j = 0; j < sizeof(test) / sizeof(*test); j++) {
		if (!test[j]) {
			PrintT("Failure at %i\n", j);
			return false;
		}
	}

	return true;
}