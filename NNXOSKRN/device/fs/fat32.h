#ifndef NNX_FAT32_HEADER
#define NNX_FAT32_HEADER
#include "fat.h"

#pragma pack(push, 1)

typedef struct
{
	UINT32 SectorFatSize32;
	UINT16 ExtFlags;
	UINT16 FilesystemVersion;
	UINT32 FirstAccessibleCluster;
	UINT16 SectorFsInfoSize16;
	UINT16 SectorMirrorSize16;
	BYTE reserved0[12];
	BYTE BiosIntNumber;
	BYTE reserved1;
	BYTE HasNameOrID;
	union
	{
		UINT32 VolumeSerialNumber;
		UINT32 VolumeID;
	};
	BYTE VolumeLabel[11];
	BYTE FatTypeInfo[8];
}BPB_EXT_FAT32, BPB32;
#pragma pack(pop)
#endif