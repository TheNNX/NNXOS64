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

UINT32 FATCalculateFATClusterCount(BPB* bpb) {

	UINT32 rootDirSectors = ((bpb->rootEntryCount * 32) + (bpb->bytesPerSector - 1)) / bpb->bytesPerSector;
	UINT32 volumeTotalSize = FATVolumeTotalSize(bpb);
	UINT32 fatSize = FATFileAllocationTableSize(bpb); 
	UINT32 dataSectorSize = volumeTotalSize - bpb->sectorReservedSize - bpb->numberOfFATs * fatSize - rootDirSectors;
	
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

bool FAT12IsFree(UINT32 n, BPB* bpb, VFS* filesystem, BYTE* sectorsData, UINT32* currentSector) {
	UINT32 mainFAT = FATLocateMainFAT(bpb);
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

	UINT8 entry = bytesContainingTheEntry;

	if (entryBitOffset % 8) {
		entry = bytesContainingTheEntry << 4;
	}

	return (entry == 0);
}

bool FAT16IsFree(UINT32 n, BPB* bpb, VFS* filesystem, BYTE* sectorsData, UINT32* currentSector) {
	UINT32 mainFAT = FATLocateMainFAT(bpb);
	UINT16 entry;
	UINT32 entryByteOffset = n * 2;

	UINT32 relativeByteOffset = entryByteOffset % bpb->bytesPerSector;
	UINT32 desiredLowSector = entryByteOffset / bpb->bytesPerSector + mainFAT;

	if (*currentSector != desiredLowSector) {
		VFSReadSector(filesystem, desiredLowSector, sectorsData);
		
		*currentSector = desiredLowSector;
	}

	entry = *((UINT16*)(sectorsData + relativeByteOffset));
	return (entry == 0);
}

bool FAT32IsFree(UINT32 n, BPB* bpb, VFS* filesystem, BYTE* sectorsData, UINT32* currentSector) {
	UINT32 mainFAT = FATLocateMainFAT(bpb);
	UINT32 entry;
	UINT32 entryByteOffset = n * sizeof(entry);

	UINT32 relativeByteOffset = entryByteOffset % bpb->bytesPerSector;
	UINT32 desiredLowSector = entryByteOffset / bpb->bytesPerSector + mainFAT;

	if (*currentSector != desiredLowSector) {
		VFSReadSector(filesystem, desiredLowSector, sectorsData);

		*currentSector = desiredLowSector;
	}

	entry = *((UINT32*)(sectorsData + relativeByteOffset));
	return (entry == 0);
}

UINT32 FATScanFree(VFS* filesystem) {
	BPB _bpb;
	BPB *bpb = &_bpb;
	VFSReadSector(filesystem, 0, bpb);
	UINT32 fatSize = FATFileAllocationTableSize(bpb);
	bool isFAT16 = FATisFAT16(bpb);
	
	UINT8 *sectorsData = nnxmalloc(((bpb->rootEntryCount == 0 || ((bpb->rootEntryCount != 0) && isFAT16)) ? 1 : 2) * bpb->bytesPerSector);
	UINT32 currentSector = 0;
	UINT32 clusterCount = FATCalculateFATClusterCount(bpb);
	UINT32 freeClusters = 0;
	 
	for (int currentEntry = 1; currentEntry < clusterCount + 2; currentEntry++) {

		if (bpb->rootEntryCount != 0) {
			if (isFAT16) {
				freeClusters += FAT16IsFree(currentEntry, bpb, filesystem, sectorsData, &currentSector);
			}
			else {
				freeClusters += FAT12IsFree(currentEntry, bpb, filesystem, sectorsData, &currentSector);
			}
		}
		else {
			freeClusters += FAT32IsFree(currentEntry, bpb, filesystem, sectorsData, &currentSector);
		}
	}

	nnxfree(sectorsData);
	return freeClusters;
}

