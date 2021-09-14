#ifndef NNX_FAT_HEADER
#define NNX_FAT_HEADER
#include <nnxtype.h>
#include "vfs.h"

// TODO TODO TODO TODO

#define FAT32_RESERVED_CLUSTER_START 0xFFFFFF8
#define FAT16_RESERVED_CLUSTER_START 0xFFF8
#define FAT12_RESERVED_CLUSTER_START 0xFF8

#define FAT_FILE_DELETED 0xE5

#pragma pack(push, 1)

typedef struct
{
	UINT16 Year : 7;
	UINT16 Month : 4;
	UINT16 Day : 5;
}FAT_DATE;

typedef struct
{
	UINT16 Hour : 5;
	UINT16 Minutes : 6;
	UINT16 Seconds : 5;
}FAT_TIME;

#define FAT_READONLY 1
#define FAT_HIDDEN 2
#define FAT_SYSTEM 4
#define FAT_VOLUME_ID 8
#define FAT_DIRECTORY 16
#define FAT_ARCHIVE 32
#define FAT_LFN (1|2|4|8);

typedef struct BPB{
	UINT8 ReservedJump[3];
	UINT8 OemName[8];
	UINT16 BytesPerSector;
	UINT8 SectorsPerCluster;
	UINT16 SectorReservedSize;
	UINT8 NumberOfFats;
	UINT16 RootEntryCount;
	UINT16 SectorTotSize16;
	UINT8 MediaType;
	UINT16 SectorFatSize16;
	UINT16 SectorsPerTrack;
	UINT16 HeadsVolumeSize;
	UINT32 SectorsHidden;
	UINT32 SectorTotSize32;
	UINT8 _[476];
}BPB, BIOS_PARAMETER_BLOCK;

typedef struct
{
	BYTE BiosIntNumber;
	BYTE reserved0;
	BYTE HasNameOrID;
	union
{
		UINT32 VolumeSerialNumber;
		UINT32 VolumeID;
	};
	BYTE VolumeLabel[11];
	BYTE FatTypeInfo[8];
}BPB_EXT_FAT1X, BPB1X;

typedef struct FAT_DIRECTORY_ENTRY{
	unsigned char Filename[8];
	unsigned char FileExtension[3];
	BYTE FileAttributes;
	BYTE Reserved;
	BYTE CreateTimeHRes;
	FAT_TIME CreationTime;
	FAT_DATE CreationDate;
	FAT_DATE AccessDate;
	UINT16 HighCluster;
	FAT_TIME ModifiedTime;
	FAT_DATE ModifiedDate;
	UINT16 LowCluster;
	UINT32 FileSize;
}FAT_DIRECTORY_ENTRY;

BOOL FatIsFileOrDir(FAT_DIRECTORY_ENTRY* sectorData);
UINT32 FatCalculateFatClusterCount(BPB* bpb);
BOOL FatIsFat12(BPB* bpb);
BOOL FatIsFat16(BPB* bpb);
BOOL FatIsFat32(BPB* bpb);
UINT32 FatFileAllocationTableSize(BPB* bpb);
UINT32 FatFollowClusterChainToAPoint(BPB* bpb, VFS* vfs, UINT32 start, UINT32 endIndex);
UINT32 FatVolumeTotalSize(BPB* bpb);
UINT32 FatScanFree(VFS* filesystem);
BOOL FatIsFree(UINT32 n, BPB* bpb, VFS* filesystem, BYTE* sectorsData, UINT32* currentSector);
UINT64 FatReadSectorOfCluster(BPB* bpb, VFS* filesystem, UINT32 clusterIndex, UINT32 sectorIndex, BYTE* data);
UINT32 FatCalculateFirstClusterPosition(BPB* bpb);
UINT64 FatSearchForFileInDirectory(FAT_DIRECTORY_ENTRY* sectorData, BPB* bpb, VFS* filesystem, const char * name, FAT_DIRECTORY_ENTRY* output);
UINT32 FatFollowClusterChain(BPB* bpb, VFS* vfs, UINT32 n);
UINT64 FatRemoveTrailingClusters(BPB* bpb, VFS* vfs, UINT32 start, UINT32 remove);
UINT64 FatAppendTrailingClusters(BPB* bpb, VFS* vfs, UINT32 start, UINT32 append);
UINT64 GetFileNameAndExtensionFromPath(const char * path, char* name, char* extension);
UINT64 FatDeleteFileEntry(BPB* bpb, VFS* vfs, FAT_DIRECTORY_ENTRY* parentDirectory, const char * filename);
UINT64 FatDeleteFile(BPB* bpb, VFS* vfs, FAT_DIRECTORY_ENTRY* parentDirectory, const char * filename);
BOOL FatCompareEntries(FAT_DIRECTORY_ENTRY* entry1, FAT_DIRECTORY_ENTRY* entry2);
UINT64 FatResizeFile(BPB* bpb, VFS* filesystem, FAT_DIRECTORY_ENTRY* parentFile, const char * filename, UINT64 newSize);
VFS_FILE* FatVfsInterfaceOpenFile(VFS* vfs, const char * path);
VOID FatVfsInterfaceCloseFile(VFS_FILE* file);
UINT64 FatVfsInterfaceWriteFile(VFS_FILE* file, UINT64 size, VOID* buffer);
UINT64 FatVfsInterfaceReadFile(VFS_FILE* file, UINT64 size, VOID* buffer);
UINT64 FatVfsInterfaceAppendFile(VFS_FILE* file, UINT64 size, VOID* buffer);
UINT64 FatVfsInterfaceResizeFile(VFS_FILE* file, UINT64 newsize);
UINT64 FatVfsInterfaceDeleteFile(VFS_FILE* file);
UINT64 FatVfsInterfaceDeleteAndCloseFile(VFS_FILE* file);
UINT64 FatVfsInterfaceRecreateDeletedFile(VFS_FILE* file);
UINT32 FatReadFatEntry(BPB* bpb, VFS* filesystem, UINT32 n, BYTE* sectorsData, UINT32* currentSector);
VOID FatInitVfs(VFS* partition);
VFS_FUNCTION_SET FatVfsInterfaceGetFunctionSet();

BOOL NNXFatAutomaticTest(VFS* filesystem);

typedef struct FATFilesystemSpecificData
{
	VOID* cachedFATSector;
	UINT32 cachedFATSectorNumber;
}FATFilesystemSpecificData;

#pragma pack(pop)
#endif
