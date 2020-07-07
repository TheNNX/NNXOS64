#ifndef NNX_FAT32_HEADER
#define NNX_FAT32_HEADER
#include "fat.h"

#pragma pack(push, 1)

typedef struct {
	UINT32 sectorFATSize32;
	UINT16 extFlags;
	UINT16 filesystemVersion;
	UINT32 firstAccessibleCluster;
	UINT16 sectorFINFOSize16;
	UINT16 sectorMirrorSize16;
	BYTE reserved0[12];
	BYTE biosIntNumber;
	BYTE reserved1;
	BYTE hasNameorID;
	union {
		UINT32 volumeSerialNumber;
		UINT32 volumeID;
	};
	BYTE volumeLabel[11];
	BYTE fatTypeInfo[8];
}BPB_EXT_FAT32, BPB32;
#pragma pack(pop)
#endif