#ifndef NNX_FAT_HEADER
#define NNX_FAT_HEADER
#include "nnxint.h"
#include "vfs.h"

#pragma pack(push, 1)

typedef struct {
	UINT16 year : 7;
	UINT16 month : 4;
	UINT16 day : 5;
}FatDate;

typedef struct {
	UINT16 hour : 5;
	UINT16 minutes : 6;
	UINT16 seconds : 5;
}FatTime;

#define FAT_READONLY 1
#define FAT_HIDDEN 2
#define FAT_SYSTEM 4
#define FAT_VOLUME_ID 8
#define FAT_DIRECTORY 16
#define FAT_ARCHIVE 32
#define FAT_LFN (1|2|4|8);

typedef struct {
	UINT8 reservedJump[3];
	UINT8 oemName[8];
	UINT16 bytesPerSector;
	UINT8 sectorsPerCluster;
	UINT16 sectorReservedSize;
	UINT8 numberOfFATs;
	UINT16 rootEntryCount;
	UINT16 sectorTotSize16;
	UINT8 mediaType;
	UINT16 sectorFATSize16;
	UINT16 sectorsPerTrack;
	UINT16 headsVolumeSize;
	UINT32 sectorsHidden;
	UINT32 sectorTotSize32;
	UINT8 _[476];
}BPB, BIOSParameterBlock;

typedef struct {
	BYTE biosIntNumber;
	BYTE reserved0;
	BYTE hasNameOrID;
	union {
		UINT32 volumeSerialNumber;
		UINT32 volumeID;
	};
	BYTE volumeLabel[11];
	BYTE fatTypeInfo[8];
}BPB_EXT_FAT1X, BPB1X;

UINT32 FATCalculateFATClusterCount(BPB* bpb);
BOOL FATisFAT16(BPB* bpb);
BOOL FATisFAT32(BPB* bpb);
UINT32 FATFileAllocationTableSize(BPB* bpb);
UINT32 FATVolumeTotalSize(BPB* bpb);
UINT32 FATScanFree(VFS* filesystem);
bool FAT16IsFree(UINT32 n, BPB* bpb, VFS* filesystem, BYTE* sectorsData, UINT32* currentSector);
bool FAT32IsFree(UINT32 n, BPB* bpb, VFS* filesystem, BYTE* sectorsData, UINT32* currentSector);
bool FAT12IsFree(UINT32 n, BPB* bpb, VFS* filesystem, BYTE* sectorsData, UINT32* currentSector);
#pragma pack(pop)
#endif
