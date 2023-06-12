#ifndef NNX_GPT_HEADER
#define    NNX_GPT_HEADER

#include <nnxtype.h>

#pragma pack(push, 1)

#define GPT_SIGNATURE 0x5452415020494645

typedef struct
{
    QWORD a[2];
} GUID, DQWORD;

BOOL GptCompareGuid(GUID a1, GUID a2);

extern GUID GPT_MS_BASIC_DISK;
extern GUID GPT_MS_EFI_DISK;
extern GUID GPT_EMPTY_TYPE;

typedef struct _GPT_PARTITION_HEADER
{
    UINT64 Signature;
    DWORD Revision;
    DWORD HeaderSize;
    DWORD Crc32Chesksum;
    DWORD Reserved0;
    QWORD LbaOfThisHeader;
    QWORD LbaOfMirrorHeader;
    QWORD FirstUsableBlock;
    QWORD LastUsableBlock;
    GUID Guid;
    QWORD LbaOfPartitionTable;
    DWORD NumberOfPartitionTableEntries;
    DWORD BytesPerEntry;
    DWORD Crc32ChecksumOfPartitionTable;
}GPT_PARTITION_HEADER, *PGPT_PARTITION_HEADER;

typedef struct GPT
{
    GPT_PARTITION_HEADER Header;
    BYTE Reserved[512 - sizeof(GPT_PARTITION_HEADER)];
}GPT;

typedef struct _GPT_PARTITION_ENTRY
{
    GUID TypeGuid;
    GUID UniqueGuid;
    QWORD LbaPartitionStart;
    QWORD LbaPartitionEnd;
    QWORD Attributes;
    // default size: 72, 36 * sizeof(UINT16)
    UINT16 PartitionName[0];
}GPT_PARTITION_ENTRY;

#pragma pack(pop)

#endif