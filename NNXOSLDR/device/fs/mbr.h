#ifndef NNX_MBR_HEADER
#define NNX_MBR_HEADER

#include <nnxint.h>
#include "device/hdd/hdd.h"

#define MBR_SIGNATURE 0xaa55

#pragma pack(push, 1)

typedef struct MBRPartitionTableEntry {
	BYTE attributes;
	HSC partitionStartCHS;
	BYTE partitionType;
	HSC partitionEndCHS;
	DWORD partitionStartLBA28;
	DWORD partitionSizeInSectors;
}MBRPartitionTableEntry;

typedef struct MBRTable {
	union
	{
		DWORD optionalUID;
		DWORD UID;
	};
	union
	{
		WORD optionalReserved;
		WORD Reserved;
	};

	MBRPartitionTableEntry tableEntries[4];

	WORD magicNumber;
} MBRTable;

typedef struct MBR {
	UINT8 bootstrap[440];
	MBRTable mbrtable;
} MBR;
#pragma pack(pop)
#endif