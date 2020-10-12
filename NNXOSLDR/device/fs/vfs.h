#ifndef NNX_VFS_HEADER
#define NNX_VFS_HEADER

#include "../../HAL/PCI/PCIIDE.h"
#include "../../HAL/PCI/PCI.h"
#define VFS_MAX_NUMBER 64

typedef struct FucntionSet {
	BOOL(*CheckIfFileExists)(struct VirtualFileSystem* filesystem, char* path);
	UINT64(*ReadFile)(struct VirtualFileSystem* filesystem, char* path, UINT64 position, UINT64 size, VOID* output);
	UINT64(*WriteFile)(struct VirtualFileSystem* filesystem, char* path, UINT64 position, UINT64 size, VOID* input);
	UINT64(*CreateFile)(struct VirtualFileSystem* filesystem, char* path);
} FunctionSet;

typedef struct VirtualFileSystem {
	IDEDrive* drive;
	UINT64 lbaStart;
	UINT64 sizeInSectors;
	UINT64 allocationUnitSize;
	FunctionSet functions;
}VirtualFileSystem, VFS;

#define VFS_ERR_INVALID_FILENAME		0xFFFFFFF1
#define VFS_ERR_INVALID_PATH			0xFFFFFFF2
#define VFS_ERR_INACCESSIBLE			0xFFFFFFF3
#define VFS_ERR_EOF						0xFFFFFFF4
#define VFS_ERR_NOT_A_DIRECTORY			0xFFFFFFF5
#define VFS_ERR_NOT_A_FILE				0xFFFFFFF6
#define VFS_ERR_FILE_NOT_FOUND			0xFFFFFFF7
#define VFS_NOT_ENOUGH_ROOM_FOR_WRITE	0xFFFFFFF8

void VFSInit();
UINT32 VFSAddPartition(IDEDrive* drive, UINT64 lbaStart, UINT64 partitionSize, FunctionSet functionSet);
VirtualFileSystem* VFSGetPointerToVFS(unsigned int n);
UINT64 VFSReadSector(VirtualFileSystem*, UINT64 n, BYTE* destination);
UINT64 VFSWriteSector(VirtualFileSystem*, UINT64 n, BYTE* source);

#endif