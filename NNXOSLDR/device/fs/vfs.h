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

#define VFS_ERR_INVALID_FILENAME			0xFFFF0001
#define VFS_ERR_INVALID_PATH				0xFFFF0002
#define VFS_ERR_INACCESSIBLE				0xFFFF0003
#define VFS_ERR_EOF							0xFFFF0004
#define VFS_ERR_NOT_A_DIRECTORY				0xFFFF0005
#define VFS_ERR_NOT_A_FILE					0xFFFF0006
#define VFS_ERR_FILE_NOT_FOUND				0xFFFF0007
#define VFS_ERR_NOT_ENOUGH_ROOM_FOR_WRITE	0xFFFF0008
#define VFS_ERR_READONLY					0xFFFF0009
#define VFS_ERR_FILE_ALREADY_EXISTS			0xFFFF000A

void VFSInit();
UINT32 VFSAddPartition(IDEDrive* drive, UINT64 lbaStart, UINT64 partitionSize, FunctionSet functionSet);
VirtualFileSystem* VFSGetPointerToVFS(unsigned int n);
UINT64 VFSReadSector(VirtualFileSystem*, UINT64 n, BYTE* destination);
UINT64 VFSWriteSector(VirtualFileSystem*, UINT64 n, BYTE* source);

#endif