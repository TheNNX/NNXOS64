#include "vfs.h"
#include "../../memory/nnxalloc.h"
#include "../../memory/MemoryOperations.h"
#include "fat.h"
#include "fat32.h"
#include "../../nnxosdbg.h"
#include "../../text.h"

#define DEBUG_STATUS PrintT("[%s:%i] %x\n", __FUNCTION__, __LINE__, status);

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

	return FATCalculateFATClusterCount(bpb) > 4084;
}

BOOL FATisFAT12(BPB* bpb) {
	return !(FATisFAT16(bpb) || FATisFAT32(bpb));
}

UINT32 FATLocateMainFAT(BPB* bpb) {
	return bpb->sectorReservedSize;
}

UINT32 FATGetRootCluster(BPB* bpb){
	return FATisFAT32(bpb) ? (((BPB_EXT_FAT32*)&(bpb->_))->firstAccessibleCluster) : 0;
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

	return 0;
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
	UINT32 firstSectorOfCluster = bpb->sectorsPerCluster * clusterIndex + FATCalculateFirstClusterPosition(bpb) - 2 * bpb->sectorsPerCluster;
	for (int sectorIndex = 0; sectorIndex < bpb->sectorsPerCluster; sectorIndex++) {
		UINT64 status = VFSReadSector(filesystem, firstSectorOfCluster + sectorIndex, data);
		if (status) {
			return status | (((UINT64)sectorIndex) << 32);
		}
		data += bpb->bytesPerSector;
	}
	return 0;
}

UINT64 FATReadSectorOfCluster(BPB* bpb, VFS* filesystem, UINT32 clusterIndex, UINT32 sectorIndex, BYTE* data) {
	UINT32 firstSectorOfCluster = bpb->sectorsPerCluster * clusterIndex + FATCalculateFirstClusterPosition(bpb) - 2 * bpb->sectorsPerCluster;
	
	UINT64 status = VFSReadSector(filesystem, firstSectorOfCluster + sectorIndex, data);
	if (status) {
		return status;
	}

	return 0;
}

UINT64 FATWriteSectorOfCluster(BPB* bpb, VFS* filesystem, UINT32 clusterIndex, UINT32 sectorIndex, BYTE* data) {
	UINT32 firstSectorOfCluster = bpb->sectorsPerCluster * clusterIndex + FATCalculateFirstClusterPosition(bpb) - 2 * bpb->sectorsPerCluster;

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

UINT32 FATGetFirstClusterOfFile(BPB* bpb, FATDirectoryEntry* dirEntry) {
	if (dirEntry == 0 || dirEntry == -1) {
		return 0xFFFFFFFF;
	}
	return dirEntry->lowCluster | (FATisFAT32(bpb) ? (dirEntry->highCluster << 16) : 0);
}

UINT64 FATCopyFirstFilenameFromPath(char* path, char* filenameCopy) {
	UINT64 slash = FindFirstSlash(path);

	if (slash == -1 && FindCharacterFirst(path, -1, 0) <= 12) {
		MemCopy(filenameCopy, path, FindCharacterFirst(path, -1, 0));
		filenameCopy[FindCharacterFirst(path, -1, 0)] = 0;
		return 0;
	}

	if (slash > 12) {
		PrintT("[%s] Invalid path separator position %i %s\n\n", __FUNCTION__, slash, path);
		return VFS_ERR_INVALID_PATH;
	}

	if (path[slash + 1] == 0)
		slash = 0;

	if (slash != 0) {
		for (UINT16 i = 0; i < slash; i++) {
			filenameCopy[i] = path[i];
			filenameCopy[i + 1] = 0;
		}
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

	UINT64 slash = FindFirstSlash(path);
	char filenameCopy[13];
	FATDirectoryEntry dirEntry;

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
			
			UINT64 status = FATSearchForFileInDirectory(sectorData, bpb, filesystem, filenameCopy, &directory);
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

	if (parentDirectoryCluster == 0xFFFFFFFF) {
		parentDirectoryCluster = FATGetRootCluster(bpb);
	}

	while (!FATIsClusterEOF(bpb, parentDirectoryCluster)) {
		for (UINT32 sectorIndex = 0; sectorIndex < bpb->sectorsPerCluster; sectorIndex++) {
			FATReadSectorOfCluster(bpb, filesystem, parentDirectoryCluster, sectorIndex, sectorData);
			UINT64 status = FATSearchForFileInDirectory(sectorData, bpb, filesystem, filenameCopy, &dirEntry);

			if (status == VFS_ERR_FILE_NOT_FOUND)
				continue;
			else if (status){
				NNXAllocatorFree(sectorData);
				return status;
			}

			if (slash != -1) {
				if ((dirEntry.fileAttributes & FAT_DIRECTORY) == 0) {
					NNXAllocatorFree(sectorData);
					return VFS_ERR_NOT_A_DIRECTORY;
				}

				UINT32 fisrtClusterOfFile = FATGetFirstClusterOfFile(bpb, &dirEntry);

				NNXAllocatorFree(sectorData);

				return FATReccursivlyFindDirectoryEntry(bpb, filesystem, fisrtClusterOfFile, path + slash + 1, fileDir);
			}
			else {
				*fileDir = dirEntry;
				NNXAllocatorFree(sectorData);
				
				return 0;
			}
		}
		parentDirectoryCluster = FATFollowClusterChain(bpb, filesystem, parentDirectoryCluster);
	}
	NNXAllocatorFree(sectorData);
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
	return 0xFFFFFFFF;
}


UINT64 FATReadSectorFromFile(BPB* bpb, VFS* vfs, FATDirectoryEntry* file, UINT32 offsetSector, PVOID output) {
	if (file == 0) {
		return VFS_ERR_FILE_NOT_FOUND;
	}

	UINT64 status = 0;
	UINT32 curCluster = FATGetFirstClusterOfFile(bpb, file);
	UINT32 clusterNumberPreceding = offsetSector / bpb->sectorsPerCluster;

	while (clusterNumberPreceding) {
		curCluster = FATFollowClusterChain(bpb, vfs, curCluster);
		if (FATIsClusterEOF(bpb, curCluster)) { 
			PrintT("%i %i %i", offsetSector, offsetSector / bpb->sectorsPerCluster, clusterNumberPreceding);
			DEBUG_STATUS;
			return VFS_ERR_EOF;
		}
		if (clusterNumberPreceding == 0)
			break;
		clusterNumberPreceding--;
	}
	UINT64 curSector = offsetSector % bpb->sectorsPerCluster;
	UINT8 buffer[4096];

	if (status = FATReadSectorOfCluster(bpb, vfs, curCluster, curSector, buffer)) {
		return status;
	}

	for (UINT32 index = 0; index < bpb->bytesPerSector; index++) {
		((UINT8*)output)[index] = buffer[index];
	}

	return 0;
}

UINT64 FATWriteSectorsToFile(BPB* bpb, VFS* vfs, FATDirectoryEntry* file, UINT32 index, PVOID input, UINT32 sectorsCount) {
	UINT32 currentCluster = FATGetFirstClusterOfFile(bpb, file);
	UINT64 status;
	UINT32 firstSector = index;
	UINT32 lastSector = index + sectorsCount - 1;

	UINT32 firstCluster = firstSector / bpb->sectorsPerCluster;
	UINT32 lastCluster = lastSector / bpb->sectorsPerCluster;
	UINT32 currentClusterIndex = firstCluster;
	
	UINT64 inputOffset = 0;

	currentCluster = FATFollowClusterChainToAPoint(bpb, vfs, currentCluster, firstCluster);

	while (currentClusterIndex <= lastCluster) {

		if (FATIsClusterEOF(bpb, currentCluster)) {
			status = VFS_ERR_EOF;
			DEBUG_STATUS;
			return VFS_ERR_EOF;
		}

		UINT32 i;

		for (i = 0; i < bpb->sectorsPerCluster; i++) {
			if (i + currentClusterIndex * bpb->sectorsPerCluster > lastSector)
				return 0;
			if (i + currentClusterIndex * bpb->sectorsPerCluster >= firstSector) {
				if (status = FATWriteSectorOfCluster(bpb, vfs, currentCluster, i, ((UINT64)input) + inputOffset)) {
					DEBUG_STATUS;
				}

				inputOffset += bpb->bytesPerSector;
			}
		}

		currentClusterIndex++;
		currentCluster = FATFollowClusterChain(bpb, vfs, currentCluster);
	}

	return 0;
}

UINT64 FATWriteFile(BPB* bpb, VFS* vfs, FATDirectoryEntry* file, UINT64 offset, UINT64 size, PVOID input, UINT32* writtenBytes) {
	UINT32 startSector = offset / bpb->bytesPerSector;
	UINT32 endSector = (offset + size - 1) / bpb->bytesPerSector;
	UINT32 sectorOffset = offset % bpb->bytesPerSector;
	UINT32 lastSectorEndOffset = (offset + size - 1) % bpb->bytesPerSector;
	UINT8 *temporarySectorData = NNXAllocatorAlloc(bpb->bytesPerSector);
	UINT32 i, localWrittenBytes;

	if (file == 0) {
		NNXAllocatorFree(temporarySectorData);
		return VFS_ERR_FILE_NOT_FOUND;
	}

	if (writtenBytes == 0)
		writtenBytes = &localWrittenBytes;
	*writtenBytes = 0;

	if (!lastSectorEndOffset)
		lastSectorEndOffset = bpb->bytesPerSector - 1;

	for (i = startSector; i <= endSector; i++) {

		UINT32 status;
		if (i == endSector || (i == startSector && sectorOffset)) {
			UINT32 lowerBound, upperBound, j;
			FATReadSectorFromFile(bpb, vfs, file, i, temporarySectorData);
			
			lowerBound = (i == startSector) ? sectorOffset : 0;
			upperBound = (i == endSector) ? lastSectorEndOffset + 1: bpb->bytesPerSector;

			for (j = 0; j < upperBound - lowerBound; j++) {
				temporarySectorData[j + lowerBound] = ((UINT8*)input)[(*writtenBytes)++];
			}
			status = FATWriteSectorsToFile(bpb, vfs, file, i, temporarySectorData, 1);
			if(status)
				DEBUG_STATUS;
			
		}
		else {
			status = FATWriteSectorsToFile(bpb, vfs, file, i, ((UINT8*)input) + *writtenBytes, 1);
			if(status)
				DEBUG_STATUS;
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

UINT64 FATAPIGetDirectoryEntry(VFS* filesystem, char * path, FATDirectoryEntry* theFile, BPB* bpb) {
	VFSReadSector(filesystem, 0, bpb);
	UINT32 rootCluster = FATGetRootCluster(bpb);
	UINT64 status = FATReccursivlyFindDirectoryEntry(bpb, filesystem, rootCluster, path, theFile);
	return status;
}

BOOL FATAPICheckIfFileExists(VFS* filesystem, char* path) {
	FATDirectoryEntry theFile;
	BPB _bpb, *bpb = &_bpb;
	UINT64 status = FATAPIGetDirectoryEntry(filesystem, path, &theFile, bpb);
	return !status;
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
	unsigned char* buffer = NNXAllocatorAlloc(bpb->bytesPerSector);
	UINT64 status = 0;
	UINT32 offset = 0;
	BOOL done = FALSE;
	FATDirectoryEntry pseudoParent;

	if (!parent || parent == 0xFFFFFFFF) {
		if (FATisFAT32(bpb)) {
			parent = &pseudoParent;
			parent->highCluster = (WORD)((FATGetRootCluster(bpb) & 0xFFFF0000) >> 16);
			parent->lowCluster = (WORD)(FATGetRootCluster(bpb) & 0xFFFF);
		}
		else {
			//TODO
			PrintT("Unimplemented\n");
			DEBUG_STATUS;
			while (1);
		}
	}

	while (!done && ((status = FATReadSectorFromFile(bpb, filesystem, parent, offset, buffer)) == 0)) {
		if (status) {
			DEBUG_STATUS;
			break;
		}
		
		for (FATDirectoryEntry* currentEntry = buffer;
			currentEntry < ((FATDirectoryEntry*)buffer) + (bpb->bytesPerSector) / sizeof(FATDirectoryEntry);
			currentEntry++)
		{

			if (((fileEntry->filename[0] == FAT_FILE_DELETED || fileEntry->filename[0] == 0) && 
				(currentEntry->filename[0] == FAT_FILE_DELETED || currentEntry->filename[0] == 0))
				|| FATCompareEntries(fileEntry, currentEntry))
			{
				*currentEntry = *desiredFileEntry;
				parent->fileSize = 0xFFFFFFFF; // bypass the size checks (if any)
				status = FATWriteSectorsToFile(bpb, filesystem, parent, offset, buffer, 1);
				if (status) {
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

UINT64 FATAddDirectoryEntry(BPB* bpb, VFS* filesystem, FATDirectoryEntry* parent, FATDirectoryEntry* fileEntry) {
	FATDirectoryEntry empty = { 0 };
	return FATChangeDirectoryEntry(bpb, filesystem, parent, &empty, fileEntry);
}

UINT64 FATAPICreateFile(VFS* filesystem, char* path) {
	FATDirectoryEntry fileEntry = { 0 };
	UINT64 borderPoint = GetFileNameAndExtensionFromPath(path, fileEntry.filename, fileEntry.fileExtension);
	UINT64 filenameLength = FindCharacterFirst(fileEntry.filename, -1, 0);
	UINT64 extensionLength = FindCharacterFirst(fileEntry.fileExtension, -1, 0);

	FATDirectoryEntry parent = { 0 };
	
	if (FATAPICheckIfFileExists(filesystem, path))
		return VFS_ERR_FILE_ALREADY_EXISTS;

	BPB _bpb, *bpb = &_bpb;

	if (borderPoint) {
		UINT64 status;
		char *parentPath = NNXAllocatorAlloc(borderPoint);

		for (UINT32 i = 0; i < borderPoint - 1; i++) {
			parentPath[i] = path[i];
		}

		parentPath[borderPoint - 1] = 0;
		FATAPIGetDirectoryEntry(filesystem, parentPath, &parent, bpb);
		if (parent.highCluster == 0 && parent.lowCluster == 0) {
			UINT32 cluster = FATFindFreeCluster(bpb, filesystem);

			if (status = FATWriteFATEntry(bpb, filesystem, cluster, 0, 0, FATGetClusterEOF(bpb))) {
				NNXAllocatorFree(parentPath);
				DEBUG_STATUS;
				return status;
			}
		}
		UINT32 parentCluster = FATGetFirstClusterOfFile(bpb, &parent);
		status = FATAddDirectoryEntry(bpb, filesystem, &parent, &fileEntry);
		if (status == VFS_ERR_EOF) {
			status = FATAppendTrailingClusters(bpb, filesystem, parentCluster, 1);
			if (status == 0) {
				status = FATAddDirectoryEntry(bpb, filesystem, &parent, &fileEntry);
				if(status)
					DEBUG_STATUS;
			}
			else {
				status = VFS_ERR_NOT_ENOUGH_ROOM_FOR_WRITE;
				DEBUG_STATUS;
			}
		}

		NNXAllocatorFree(parentPath);
		return status;
	}
	else {
		UINT64 status;
		if (!(status = VFSReadSector(filesystem, 0, bpb)))
		{
			if (status = FATAddDirectoryEntry(bpb, filesystem, 0, &fileEntry))
				return status;
		}
		else {
			return status;
		}
	}
}

VFSFile* FATAPIOpenFile(VFS* vfs, char* path) {
	BPB _bpb, *bpb = &_bpb;
	FATDirectoryEntry direntry;
	UINT64 status;

	if (status = FATAPIGetDirectoryEntry(vfs, path, &direntry, bpb)) {
		PrintT("[%s:%i] Pseudoopen failed\n", __FILE__, __LINE__);
		return 0;
	}


	VFSFile* file = VFSAllocateVFSFile(vfs, path);
	file->fileSize = direntry.fileSize;
	return file;
}

VOID FATAPICloseFile(VFSFile* file) {
	VFSDeallocateVFSFile(file);
}

UINT64 FATAPIRecreateDeletedFile(VFSFile* file) {
	return FATAPICreateFile(file->filesystem, file->path);
}

UINT64 FATAPIDeleteFileFromPath(VFS* filesystem, char* path) {
	BPB _bpb, *bpb = &_bpb;
	UINT64 status;
	FATDirectoryEntry theFile, parentFile;
	UINT64 parentPathLength = GetParentPathLength(path) + 1;

	if (status = FATAPIGetDirectoryEntry(filesystem, path, &theFile, bpb)) {
		return status;
	}

	if (parentPathLength != 0) {
		char* parentPath = NNXAllocatorAlloc(parentPathLength);
		GetParentPath(path, parentPath);

		if (status = FATAPIGetDirectoryEntry(filesystem, parentPath, &parentFile, bpb)) {
			NNXAllocatorFree(parentPath);
			return status;
		}

		if (status = FATDeleteFile(bpb, filesystem, &parentFile, path + parentPathLength)) {
			NNXAllocatorFree(parentPath);
			return status;
		}
		NNXAllocatorFree(parentPath);
	}
	else {
		if (status = FATDeleteFile(bpb, filesystem, 0, path)) {
			return status;
		}
	}

	return 0;
}


UINT64 FATAPIDeleteFile(VFSFile* file) {
	FATAPIDeleteFileFromPath(file->filesystem, file->path);
}

UINT64 FATAPIDeleteAndCloseFile(VFSFile* file) {
	UINT64 status = FATAPIDeleteFile(file);
	FATAPICloseFile(file);
	return status;
}

UINT64 Checks(VFSFile* file, UINT64 size, VOID* buffer) {
	if (size == 0)
		return VFS_ERR_ARGUMENT_INVALID;
	if (buffer == 0)
		return VFS_ERR_ARGUMENT_INVALID;
	if (file == 0)
		return VFS_ERR_ARGUMENT_INVALID;
	if (file->path == 0)
		return VFS_ERR_INVALID_PATH;
	if (file->name == 0)
		return VFS_ERR_INVALID_FILENAME;
	if (file->fileSize < file->filePointer)
		return VFS_ERR_ARGUMENT_INVALID;

	return 0;
}

UINT64 FATAPIWriteFile(VFSFile* file, UINT64 size, VOID* buffer) {
	FATDirectoryEntry direntry;
	BPB _bpb, *bpb = &_bpb;
	UINT64 status;
	UINT32 written;

	if (status = Checks(file, size, buffer)) {
		DEBUG_STATUS;
		return status;
	}

	if (status = FATAPIGetDirectoryEntry(file->filesystem, file->path, &direntry, bpb)) {
		DEBUG_STATUS;
		return status;
	}
	
	status = FATWriteFile(bpb, file->filesystem, &direntry, file->filePointer, size, buffer, &written);
	file->filePointer += written;
	
	if(status)
		DEBUG_STATUS;
	return status;
}

UINT64 FATAPIReadFile(VFSFile* file, UINT64 size, VOID* buffer) {
	FATDirectoryEntry direntry;
	BPB _bpb, *bpb = &_bpb;
	UINT64 status;
	UINT32 read;

	if (status = Checks(file, size, buffer))
		return status;

	if (status = FATAPIGetDirectoryEntry(file->filesystem, file->path, &direntry, bpb))
		return status;

	status = FATReadFile(bpb, file->filesystem, &direntry, file->filePointer, size, buffer, &read);
	file->filePointer += read;

	if (status)
		return status;

	return 0;
}

UINT64 FATAPIAppendFile(VFSFile* file, UINT64 size, VOID* buffer) {
	FATDirectoryEntry direntry;
	BPB _bpb, *bpb = &_bpb;
	UINT64 status;
	UINT32 written;

	if (status = Checks(file, size, buffer)) {
		PrintT("[%s] Checks failed\n", __FUNCTION__);
		return status;
	}

	if (status = FATAPIResizeFile(file, file->fileSize + size)) {
		PrintT("[%s] Resize failed\n", __FUNCTION__);
		return status;
	}

	if (status = FATAPIGetDirectoryEntry(file->filesystem, file->path, &direntry, bpb)) {
		PrintT("[%s] Pseudoopen failed\n", __FUNCTION__);
		return status;
	}

	return FATWriteFile(bpb, file->filesystem, &direntry, file->fileSize - size, size, buffer, &written);
}

UINT64 FATAPIResizeFile(VFSFile* file, UINT64 newsize) {
	FATDirectoryEntry direntry, parentDirentry;
	BPB _bpb, *bpb = &_bpb;
	UINT64 status;
	VFSReadSector(file->filesystem, 0, bpb);

	UINT64 parentPathLength = GetParentPathLength(file->path) + 1;

	if (parentPathLength > 0) {
		char* parentPath = NNXAllocatorAlloc(parentPathLength);
		GetParentPath(file->path, parentPath);
		status = FATAPIGetDirectoryEntry(file->filesystem, parentPath, &parentDirentry, bpb);
		if (status) {
			PrintT("[%s] Pseudoopen failed\n", __FUNCTION__);
			NNXAllocatorFree(parentPath);
			return status;
		}

		NNXAllocatorFree(parentPath);

		
		status = FATResizeFile(bpb, file->filesystem, &parentDirentry, file->name, newsize);

		if (status) {
			PrintT("[%s:%i] Resize failed\n", __FUNCTION__, __LINE__);
			return status;
		}
		file->fileSize = newsize;
	}
	else {
		status = FATResizeFile(bpb, file->filesystem, 0, file->name, newsize);

		if (status) {
			PrintT("[%s:%i] Resize failed\n", __FUNCTION__, __LINE__);
			return status;
		}
		file->fileSize = newsize;
	}

	return 0;
}

/*
	UINT64(*CreateDirectory)(struct VirtualFileSystem* filesystem, char* path);
	UINT64(*MoveFile)(char* oldPath, char* newPath);
	UINT64(*RenameFile)(VFSFile* file, char* newFileName);
*/

UINT64 FATAPIChangeDirectoryEntry(VFS* vfs, char* path, FATDirectoryEntry* desiredFileEntry, BPB* bpb) {
	UINT64 parentPathLength, status;
	FATDirectoryEntry fileEntry;

	if (status = FATAPIGetDirectoryEntry(vfs, path, &fileEntry, bpb))
		return status;

	parentPathLength = GetParentPathLength(path);
	if (parentPathLength > 0) {
		FATDirectoryEntry parentFile;
		char* parentPath = NNXAllocatorAlloc(parentPathLength + 1);
		GetParentPath(path, parentPath);
		if (status = FATAPIGetDirectoryEntry(vfs, parentPath, &parentFile, bpb)) {
			NNXAllocatorFree(parentPath);
			DEBUG_STATUS;
			return status;
		}

		if (status = FATChangeDirectoryEntry(bpb, vfs, &parentFile, &fileEntry, desiredFileEntry)) {
			NNXAllocatorFree(parentPath);
			DEBUG_STATUS;
			return status;
		}
		NNXAllocatorFree(parentPath);
	}
	else {
		if (status = FATChangeDirectoryEntry(bpb, vfs, 0, &fileEntry, desiredFileEntry)) {
			DEBUG_STATUS;
			return status;
		}
	}

	return 0;
}

UINT64 FATAPIChangeFileAttributes(VFS* vfs, char* path, BYTE attributes) {
	UINT64 parentPathLength, status;
	BPB _bpb, *bpb = &_bpb;
	FATDirectoryEntry fileEntry;

	if (status = FATAPIGetDirectoryEntry(vfs, path, &fileEntry, bpb))
	{
		DEBUG_STATUS;
		return status;
	}

	fileEntry.fileAttributes = attributes;

	if (status = FATAPIChangeDirectoryEntry(vfs, path, &fileEntry, bpb)) {
		DEBUG_STATUS;
		return status;
	}

	return 0;
}

BYTE FATAPIGetFileAttributes(VFS* vfs, char* path) {
	BPB _bpb, *bpb = &_bpb;
	FATDirectoryEntry theFile;
	FATAPIGetDirectoryEntry(vfs, path, &theFile, bpb);
	return theFile.fileAttributes;
}

UINT64 FATAPICreateDirectory(VFS* vfs, char* path) {
	BPB _bpb, *bpb = &_bpb;
	FATDirectoryEntry fileEntry;
	FATDirectoryEntry selfEntry, parentEntry;

	/* Remove trailing slashes, if any*/
	UINT64 pathLength = FindCharacterFirst(path, -1, 0);
	UINT64 status, parentPathLength, parentCluster;

	char* pathCopy = NNXAllocatorAlloc(pathLength + 1);

	pathCopy[pathLength] = 0;
	while (pathLength & (pathCopy[pathLength - 1] == '/' || pathCopy[pathLength - 1] == '\\')) {
		pathCopy[pathLength - 1] = 0;
		pathLength--;
	}

	for (UINT64 i = 0; i < pathLength; i++) {
		pathCopy[i] = path[i];
	}
	/* Ignore the original path, replace the pointer with the pointer to the local (trailing-slashless) copy */
	path = pathCopy;


	if (status = FATAPICreateFile(vfs, path))
	{
		NNXAllocatorFree(path);
		DEBUG_STATUS;
		return status;
	}

	if (status = FATAPIChangeFileAttributes(vfs, path, FAT_DIRECTORY))
	{
		NNXAllocatorFree(path);
		DEBUG_STATUS;
		return status;
	}

	if (status = FATAPIGetDirectoryEntry(vfs, path, &fileEntry, bpb)) {
		UINT64 status2;
		if (status2 = FATAPIChangeFileAttributes(vfs, path, 0)) {
			NNXAllocatorFree(path);
			DEBUG_STATUS;
			return status;
		}
		if (status2 = FATAPIDeleteFileFromPath(vfs, path)) {
			NNXAllocatorFree(path);
			DEBUG_STATUS;
			return status;
		}

		NNXAllocatorFree(path);
		DEBUG_STATUS;
		return status;
	}

	parentEntry = fileEntry;
	selfEntry = fileEntry;

	MemCopy(parentEntry.filename, "..      ", 8);
	MemCopy(selfEntry.filename, ".       ", 8);
	MemCopy(parentEntry.fileExtension, "   ", 3);
	MemCopy(selfEntry.fileExtension, "   ", 3);

	parentPathLength = GetParentPathLength(path);
	if (parentPathLength)
	{
		FATDirectoryEntry tempParent;
		char* parentPath = NNXAllocatorAlloc(parentPathLength + 1);
		GetParentPath(path, parentPath);

		if (status = FATAPIGetDirectoryEntry(vfs, parentPath, &tempParent, bpb)) {
			NNXAllocatorFree(parentPath);
			DEBUG_STATUS;
			return status;
		}

		parentCluster = FATGetFirstClusterOfFile(bpb, &tempParent);
		NNXAllocatorFree(parentPath);
	}
	else {
		parentCluster = FATGetRootCluster(bpb);
	}

	parentEntry.lowCluster = parentCluster & 0xFFFF;
	parentEntry.highCluster = (parentCluster & 0xFFFF0000) >> 16;

	FATAddDirectoryEntry(bpb, vfs, &fileEntry, &selfEntry);
	FATAddDirectoryEntry(bpb, vfs, &fileEntry, &parentEntry);
	
	NNXAllocatorFree(path);
	if(status)
		DEBUG_STATUS;
	return status;
}

VFSFunctionSet FATAPIGetFunctionSet() {
	VFSFunctionSet functionSet = { 0 };
	functionSet.CheckIfFileExists = FATAPICheckIfFileExists;
	functionSet.OpenFile = FATAPIOpenFile;
	functionSet.CloseFile = FATAPICloseFile;
	functionSet.ReadFile = FATAPIReadFile;
	functionSet.WriteFile = FATAPIWriteFile;
	functionSet.AppendFile = FATAPIAppendFile;
	functionSet.CreateFile = FATAPICreateFile;
	functionSet.RecreateDeletedFile = FATAPIRecreateDeletedFile;
	functionSet.ResizeFile = FATAPIResizeFile;
	functionSet.DeleteFile = FATAPIDeleteFile;
	functionSet.DeleteAndCloseFile = FATAPIDeleteAndCloseFile;
	functionSet.CreateDirectory = FATAPICreateDirectory;
	return functionSet;
}

FATDirectoryEntry FATEntryFromPath(char* path) {
	FATDirectoryEntry result = { 0 };
	GetFileNameAndExtensionFromPath(path, result.filename, result.fileExtension);
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
	UINT32 numberOfClustersScanned = 0;
	while (FATIsClusterEOF(bpb, start) == false && numberOfClustersScanned < endIndex) {
		start = FATFollowClusterChain(bpb, vfs, start);
		numberOfClustersScanned++;
	}

	return start;
}

UINT32 FATFollowClusterChainToEnd(BPB* bpb, VFS* vfs, UINT32 start) {
	UINT32 last = 0;
	while (FATIsClusterEOF(bpb, start) == false) {
		last = start;
		start = FATFollowClusterChain(bpb, vfs, start);
	}

	return last;
}

UINT64 FATRemoveTrailingClusters(BPB* bpb, VFS* vfs, UINT32 start, UINT32 removeFrom) {
	UINT64 status = 0;
	UINT32 clusterChainEnd = FATFollowClusterChainToAPoint(bpb, vfs, start, removeFrom - (removeFrom != 0));
	UINT32 clusterToBeRemoved = (removeFrom != 0) ? FATFollowClusterChain(bpb, vfs, clusterChainEnd) : clusterChainEnd;

	if (removeFrom) {
		if (status = FATWriteFATEntry(bpb, vfs, clusterChainEnd, 0, 0, FATGetClusterEOF(bpb))) //mark the last cluster as EOF
			return status;
	}
	while (FATIsClusterEOF(bpb, clusterToBeRemoved) == false) {
		UINT32 currentCluster = clusterToBeRemoved;
		clusterToBeRemoved = FATFollowClusterChain(bpb, vfs, currentCluster); //store the next cluster
		if (status = FATWriteFATEntry(bpb, vfs, currentCluster, 0, 0, 0)) //mark the cluster as empty
			return status;
	}
	return 0;
}

// TODO: This is slow, make it fast
// Cache the FAT table current sector and flush it when needed
// Same (but without the flushing of course) applies to FATRemoveTraliningClusters too
UINT64 FATAppendTrailingClusters(BPB* bpb, VFS* vfs, UINT32 start, UINT32 n) {
	UINT64 status = 0;
	UINT32 cluster = 0, lastCluster = FATFollowClusterChainToEnd(bpb, vfs, start);
	char empty[512] = { 0 };
	UINT32 i;

	for (i = 0; i < n; i++) {
		UINT32 j;
		cluster = FATFindFreeCluster(bpb, vfs);

		if (status = FATWriteFATEntry(bpb, vfs, cluster, 0, 0, FATGetClusterEOF(bpb)))
			return status;
		if (status = FATWriteFATEntry(bpb, vfs, lastCluster, 0, 0, cluster))
			return status;
		for (j = 0; j < bpb->sectorsPerCluster; j++) 
			FATWriteSectorOfCluster(bpb, vfs, cluster, j, empty);
		lastCluster = cluster;
	}

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
	UINT32 firstCluster = cluster;

	if (FATIsClusterEOF(bpb, cluster))
		return VFS_ERR_EOF;

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

UINT64 FATDeleteDirectoryEntry(BPB* bpb, VFS* vfs, FATDirectoryEntry* parentDirectory, FATDirectoryEntry* fatEntry) {
	UINT64 status;
	FATDirectoryEntry empty;
	char filename[14];
	UINT64 filenameIndex = 0;
	UINT64 filenameLength = 0;
	UINT64 extensionLength = 0;
	UINT64 i, readBytes;

	filenameLength = FindCharacterFirst(fatEntry->filename, 8, ' ');
	extensionLength = FindCharacterFirst(fatEntry->fileExtension, 3, ' ');

	if (filenameLength == -1)
		filenameLength = 8;

	if (extensionLength == -1)
		extensionLength = 3;

	for (i = 0; i < sizeof(empty); i++) {
		((char*)(&empty))[i] = 0;
	}

	((char*)&empty)[0] = FAT_FILE_DELETED;

	if (fatEntry->fileAttributes & FAT_DIRECTORY) {
		FATDirectoryEntry previous = { "..      ", "   " };
		FATDirectoryEntry self = { ".       ", "   " };
		UINT64 currentCluster = FATGetFirstClusterOfFile(bpb, fatEntry);
		BYTE* sectorData = NNXAllocatorAlloc(bpb->bytesPerSector);

		FATChangeDirectoryEntry(bpb, vfs, fatEntry, &previous, &empty);
		FATChangeDirectoryEntry(bpb, vfs, fatEntry, &self, &empty);

		while (currentCluster > 0 && !FATIsClusterEOF(bpb, currentCluster)) {
			UINT64 sectorIndex = 0;
			for (sectorIndex = 0; sectorIndex < bpb->sectorsPerCluster; sectorIndex++) {
				UINT64 currentEntry = 0;
				FATReadSectorOfCluster(bpb, vfs, currentCluster, sectorIndex, sectorData);
				for (currentEntry = 0; currentEntry < bpb->bytesPerSector / 32; currentEntry++) {
					FATDirectoryEntry* current = ((FATDirectoryEntry*)sectorData) + currentEntry;
					if (current->filename[0] != 0x00 && current->filename[0] != FAT_FILE_DELETED) {
						FATDeleteDirectoryEntry(bpb, vfs, fatEntry, current);
					}
				}
			}
			currentCluster = FATFollowClusterChain(bpb, vfs, currentCluster);
		}

		NNXAllocatorFree(sectorData);
	}

	for (i = 0; i < filenameLength; i++) {
		filename[filenameIndex++] = fatEntry->filename[i];
	}

	if (extensionLength)
		filename[filenameIndex++] = '.';

	for (i = 0; i < extensionLength; i++) {
		filename[filenameIndex++] = fatEntry->fileExtension[i];
	}

	filename[filenameIndex] = 0;

	if (status = FATResizeFile(bpb, vfs, parentDirectory, filename, 0))
		return status;

	if (status = FATReccursivlyFindDirectoryEntry(bpb, vfs, FATGetFirstClusterOfFile(bpb, parentDirectory), filename, &fatEntry))
		return status;

	if (status = FATChangeDirectoryEntry(bpb, vfs, parentDirectory, &fatEntry, &empty)) {
		PrintT("[%s:%i] 0x%X\n", __FUNCTION__, __LINE__, status);
		return status;
	}

	return 0;
}

UINT64 FATDeleteFile(BPB* bpb, VFS* vfs, FATDirectoryEntry* parentDirectory, char* filename) {
	UINT64 status;
	FATDirectoryEntry fatEntry;

	if (status = FATReccursivlyFindDirectoryEntry(bpb, vfs, FATGetFirstClusterOfFile(bpb, parentDirectory), filename, &fatEntry))
		return status;

	if (status = FATDeleteDirectoryEntry(bpb, vfs, parentDirectory, &fatEntry))
		return status;

	return 0;
}

char logFilepath[] = "efi\\LOG.TXT";
char secondFilePath[] = "efi\\TEST.TXT";
char rootDirPath[] = "ROOT.DIR";

char log1[] = "123456789";
char log2[] = "ABCDEFGHIJKLMNOPQ";

UINT64 lastMemory = 0, currentMemory = 0;
char* caller = 0;

BOOL NNX_FATAutomaticTest(VFS* filesystem) {
	//TODO: FAT16/FAT12 TESTS AND SUPPORT FOR FILESYSTEM OPERATIONS ON THE ROOT DIRECTORY
	UINT32 i = 0;
	PrintT("FAT Test for filesystem 0x%X\n", filesystem);

	for (i = 0; i < 10; i++) {
		UINT64 status, __lastMemory, __currentMemory;
		char* __caller;

		if (!filesystem->functions.CheckIfFileExists(filesystem, logFilepath)) {
			if (status = filesystem->functions.CreateFile(filesystem, logFilepath)) {
				PrintT("[%s] File creation failed.\n", __FUNCTION__);
				return FALSE;
			}
		}

		VFSFile* file = filesystem->functions.OpenFile(filesystem, logFilepath);

		if (status = filesystem->functions.AppendFile(file, sizeof(log1) - 1, log1)) {
			PrintT("[%s] Append file 1 failed\n", __FUNCTION__);
			return FALSE;
		}

		if (status = filesystem->functions.AppendFile(file, sizeof(log2) - 1, log2)) {
			PrintT("[%s] Append file 2 failed\n", __FUNCTION__);
			return FALSE;
		}

		filesystem->functions.CloseFile(file);
		
		SaveStateOfMemory("Open");
		if (filesystem->functions.CheckIfFileExists(filesystem, secondFilePath)) {
			VFSFile* file = filesystem->functions.OpenFile(filesystem, secondFilePath);

			if (file) {
				filesystem->functions.DeleteAndCloseFile(file);
			}
			else {
				PrintT("Cannot open file for deletion\n");
				return FALSE;
			}
		}
		CheckMemory();
		
		
		SaveStateOfMemory("Create");
		if (status = filesystem->functions.CreateFile(filesystem, secondFilePath)) {
			PrintT("Cannot create file %x\n", status);
			return FALSE;
		}
		CheckMemory();
		
		SaveStateOfMemory("DIRECTOR.Y");
		if (filesystem->functions.CheckIfFileExists(filesystem, "efi\\DIRECTOR.Y")) {
			VFSFile* dir = filesystem->functions.OpenFile(filesystem, "efi\\DIRECTOR.Y");
			filesystem->functions.DeleteAndCloseFile(dir);
		}

		if (status = filesystem->functions.CreateDirectory(filesystem, "efi\\DIRECTOR.Y")) {
			PrintT("Cannot create the directory. 0x%X\n", status);
			return FALSE;
		}
		CheckMemory();


		SaveStateOfMemory("rootDirPath");
		if (FATAPICheckIfFileExists(filesystem, rootDirPath)) {
			char readBuffer[9];
			readBuffer[8] = 0;
			VFSFile* rootDir = FATAPIOpenFile(filesystem, rootDirPath);
			rootDir->filePointer = 0;
			if (!(status = FATAPIWriteFile(rootDir, sizeof(log1), log1))) {
				if (!(status = FATAPIReadFile(rootDir, 8, readBuffer))) {
					PrintT("Read: %S\n", readBuffer, 8);

					if (FATAPIResizeFile(rootDir, 65536))
						PrintT("Resize 1 failed\n");

					if (FATAPIResizeFile(rootDir, 65536 / 2))
						PrintT("Resize 2 failed\n");
				}
				else {
					PrintT("Root read failed %x\n", status);
				}
			}
			else {
				PrintT("Root write failed %x\n", status);
			}

			FATAPICloseFile(rootDir);
		}
		else {
			PrintT("%s does not exist\n", rootDirPath);
		}
		CheckMemory();

		SaveStateOfMemory("DELETE.TST");
		if (FATAPICheckIfFileExists(filesystem, "DELETE.TST")) {
			if (status = FATAPIDeleteFileFromPath(filesystem, "DELETE.TST")) {
				PrintT("Failed to delete DELETE.TST %x\n", status);
			}
			else {
				PrintT("Deleted DELETE.TST\n");
			}
		}
		else {
			PrintT("DELETE.TST does not exist\n");
		}
		CheckMemory();

		SaveStateOfMemory("Create file in root");
		if (!(status = FATAPICreateFile(filesystem, "DELETE.TST"))) {
			VFSFile* file;
			if (file = FATAPIOpenFile(filesystem, "DELETE.TST")) {
				UINT64 i;
				for(i = 0; i < 8; i++)
					FATAPIAppendFile(file, 12, "DELETE.TST\xd\xa");
				FATAPICloseFile(file);
			}
			else {
				PrintT("File doesn't exist despite being created\n");
				return FALSE;
			}
		}
		CheckMemory();
		
	}
	PrintT("[%s] Test success\n", __FUNCTION__);
	return TRUE;
}