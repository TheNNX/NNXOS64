#include "vfs.h"
#include "../../memory/nnxalloc.h"
#include "../../memory/MemoryOperations.h"
#include "fat.h"
#include "fat32.h"

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

UINT32 FATLocateMainFAT(BPB* bpb) {
	return bpb->sectorReservedSize;
}

UINT32 FATReadFATEntry(UINT32 n, BPB* bpb, VFS* filesystem, BYTE* sectorsData, UINT32* currentSector) {
	UINT32 mainFAT = FATLocateMainFAT(bpb);
	UINT32 entry;
	int altCurSec = 0;
	if (currentSector == 0 || sectorsData == 0) {
		currentSector = &altCurSec;
		sectorsData = nnxmalloc(bpb->bytesPerSector);
	}

	if (FATisFAT32(bpb) || FATisFAT16(bpb)) {
		UINT32 entryByteOffset = n * 2 * (FATisFAT32(bpb) + 1);

		UINT32 relativeByteOffset = entryByteOffset % bpb->bytesPerSector;
		UINT32 desiredLowSector = entryByteOffset / bpb->bytesPerSector + mainFAT;

		if (*currentSector != desiredLowSector) {
			VFSReadSector(filesystem, desiredLowSector, sectorsData);

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
			VFSReadSector(filesystem, desiredLowSector, sectorsData);
			VFSReadSector(filesystem, desiredLowSector + 1, sectorsData + bpb->bytesPerSector);

			*currentSector = desiredLowSector;
		}

		bytesContainingTheEntry = *((UINT16*)(sectorsData + relativeLowByteOffset));

		if (entryBitOffset % 8) {
			bytesContainingTheEntry = bytesContainingTheEntry << 4;
		}

		entry = bytesContainingTheEntry;
	}

	if (currentSector == &altCurSec) {
		nnxfree(sectorsData);
	}

	return entry;
}

bool FATIsFree(UINT32 n, BPB* bpb, VFS* filesystem, BYTE* sectorsData, UINT32* currentSector) {
	return !FATReadFATEntry(n, bpb, filesystem, sectorsData, currentSector);
}

UINT32 FATFollowClusterChain(UINT32 cluster, BPB* bpb, VFS* filesystem) {
	return FATReadFATEntry(cluster, bpb, filesystem, 0, 0);
}

/**
	WARNING: not recommended for cluster sizes > 4KB (nnxalloc cannot allocate memory above 4KB page size)
	Recommended way of reading clusters is to read each sector individually using FATReadSectorOfCluster
**/
UINT32 FATReadCluster(BPB* bpb, VFS* filesystem, UINT32 clusterIndex, BYTE* data) {
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

UINT32 FATReadSectorOfCluster(BPB* bpb, VFS* filesystem, UINT32 clusterIndex, UINT32 sectorIndex, BYTE* data) {
	UINT32 firstSectorOfCluster = bpb->sectorsPerCluster * (clusterIndex - 2) + FATCalculateFirstClusterPosition(bpb);
	
	UINT64 status = VFSReadSector(filesystem, firstSectorOfCluster + sectorIndex, data);
	if (status) {
		return status;
	}

	return 0;
}

bool FATParseDir(FATDirectoryEntry* sectorData, BPB* bpb) {
	for (UINT64 entryIndex = 0; entryIndex < (bpb->bytesPerSector / 32); entryIndex++) {
		if (sectorData[entryIndex].filename[0] == 0x0) {
			return 0;
		}
		else if (sectorData[entryIndex].filename[0] == 0xe5) {
			continue;
		}
		else if (sectorData[entryIndex].fileAttributes & FAT_VOLUME_ID) {
			continue;
		}

		PrintT("%s: %S %S\n", ((sectorData[entryIndex].fileAttributes & FAT_DIRECTORY) ? ("Directory") : ("File")), sectorData[entryIndex].filename, 8, sectorData[entryIndex].fileExtension, 3);
	}

	return 1;
}

void FATDebugDirRoot(VFS* filesystem) {
	BPB _bpb, *bpb = &_bpb;
	VFSReadSector(filesystem, 0, bpb);
	bool isFAT32, isFAT16;
	isFAT32 = FATisFAT32(bpb);
	isFAT16 = FATisFAT16(bpb);

	UINT32 fatSize = FATFileAllocationTableSize(bpb);
	UINT32 startOfDirectory = bpb->sectorReservedSize + bpb->numberOfFATs * fatSize;

	if (isFAT32) {
		BPB_EXT_FAT32 *extBPB = &(bpb->_);
		UINT32 rootCluster = extBPB->firstAccessibleCluster;
		BYTE* sectorData = nnxmalloc(bpb->bytesPerSector);
		while (rootCluster < 0xFFFFFF8) {
			for (UINT32 sectorIndex = 0; sectorIndex < bpb->sectorsPerCluster; sectorIndex++) {
				FATReadSectorOfCluster(bpb, filesystem, rootCluster, sectorIndex, sectorData);
				FATParseDir(sectorData, bpb);
			}
			rootCluster = FATFollowClusterChain(rootCluster, bpb, filesystem);
		}
		nnxfree(sectorData);
	}
	else {
		bool end = false;
		UINT16 rootDirSize = bpb->rootEntryCount;
		UINT32 rootDirSectorSize = ((rootDirSize * 32) + (bpb->bytesPerSector - 1)) / bpb->bytesPerSector;
		FATDirectoryEntry* sectorData = nnxmalloc(bpb->bytesPerSector);
		for (UINT32 rootSectorIndex = 0; (rootSectorIndex < rootDirSectorSize) && !end; rootSectorIndex++) {
			VFSReadSector(filesystem, startOfDirectory + rootSectorIndex, sectorData);
			if (!FATParseDir(sectorData, bpb)) {
				break;
			}
		}
		nnxfree(sectorData);
	}
}

UINT32 FATScanFree(VFS* filesystem) {
	BPB _bpb, *bpb = &_bpb;
	VFSReadSector(filesystem, 0, bpb);
	UINT32 fatSize = FATFileAllocationTableSize(bpb);
	bool isFAT16 = FATisFAT16(bpb);
	
	UINT8 *sectorsData = nnxmalloc(((bpb->rootEntryCount == 0 || ((bpb->rootEntryCount != 0) && isFAT16)) ? 1 : 2) * bpb->bytesPerSector);
	UINT32 currentSector = 0;
	UINT32 clusterCount = FATCalculateFATClusterCount(bpb);
	UINT32 freeClusters = 0;
	 
	for (int currentEntry = 1; currentEntry < clusterCount + 2; currentEntry++) {
		freeClusters += FATIsFree(currentEntry, bpb, filesystem, sectorsData, &currentSector);
	}

	nnxfree(sectorsData);
	return freeClusters;
}

