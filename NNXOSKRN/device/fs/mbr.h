#ifndef NNX_MBR_HEADER
#define NNX_MBR_HEADER

#include <nnxtype.h>
#include "device/hdd/hdd.h"

#define MBR_SIGNATURE 0xaa55

#pragma pack(push, 1)

typedef struct _MBR_PARTITION_TABLE_ENTRY
{
	BYTE Attributes;
	HSC PartitionStartCHS;
	BYTE PartitionType;
	HSC PartitionEndCHS;
	DWORD PartitionStartLBA28;
	DWORD PartitionSizeInSectors;
}MBR_PARTITION_TABLE_ENTRY;

typedef struct _MBR_TABLE
{
	union
	{
		DWORD OptionalUID;
		DWORD UID;
	};
	union
	{
		WORD OptionalReserved;
		WORD Reserved;
	};

	MBR_PARTITION_TABLE_ENTRY TableEntries[4];

	WORD MagicNumber;
} MBR_TABLE;

typedef struct MBR
{
	UINT8 bootstrap[440];
	MBR_TABLE mbrtable;
} MBR;
#pragma pack(pop)
#endif