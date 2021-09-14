#ifndef NNX_GPT_HEADER
#define	NNX_GPT_HEADER

#include <nnxtype.h>

#pragma pack(push, 1)

#define GPT_SIGNATURE 0x5452415020494645

typedef struct { QWORD a[2]; } GUID, DQWORD;

BOOL GPTCompareGUID(GUID a1, GUID a2);

extern GUID GPT_MS_BASIC_DISK;
extern GUID GPT_MS_EFI_DISK;
extern GUID GPT_EMPTY_TYPE;

typedef struct GPTPartitionHeader
{
	UINT64 signature;
	DWORD revision;
	DWORD headerSize;
	DWORD crc32Chesksum;
	DWORD reserved0;
	QWORD lbaOfThisHeader;
	QWORD lbaOfMirrorHeader;
	QWORD firstUsableBlock;
	QWORD lastUsableBlock;
	GUID guid;
	QWORD lbaOfPartitionTable;
	DWORD numberOfPartitionTableEntries;
	DWORD bytesPerEntry;
	DWORD crc32ChecksumOfPartitionTable;
}GPTPartitionHeader, *PGPTPartitionHeader;

typedef struct GPT
{
	GPTPartitionHeader header;
	BYTE Reserved[512 - sizeof(GPTPartitionHeader)];
}GPT;

typedef struct GPTPartitionEntry
{
	GUID typeGUID;
	GUID uniqueGUID;
	QWORD lbaPartitionStart;
	QWORD lbaPartitionEnd;
	QWORD attributes;
	//default size: 72, 36 * sizeof(UINT16)
	UINT16 partitionName[0];
}GPTPartitionEntry;

#pragma pack(pop)

#endif