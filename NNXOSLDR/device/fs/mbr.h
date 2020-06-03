#ifndef NNX_MBR_HEADER
#define NNX_MBR_HEADER

#include "nnxint.h"
#include "device/hdd/hdd.h"
#pragma pack(push, 1)
typedef struct MBRTable {
	union
	{
		DWORD optionalUID;
		DWORD UID;
	};
	union
	{
		WORD optionalReserved;
		WORD reserved;
	};
	struct PartitionTableEntry {
		BYTE attributes;
		HSC partitionStartCHS;
		BYTE partitionType;
		HSC partitionEndCHS;
		DWORD partitionStartLBA28;
		DWORD partitionSizeInSectors;
	} tableEntries[4];

	WORD magicNumber;
} MBRTable;

#ifdef  __cplusplus
typedef struct MBRTable::PartitionTableEntry PartitionTableEntry;
#endif

typedef struct MBR {
	UINT8 bootstrap[440];
	MBRTable mbrtable;
} MBR;
#pragma pack(pop)
#endif