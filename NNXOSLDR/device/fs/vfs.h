#ifndef NNX_VFS_HEADER
#define NNX_VFS_HEADER

#include "../../HAL/PCI/PCIIDE.h"
#include "../../HAL/PCI/PCI.h"
#include "../../memory/nnxalloc.h"
#define VFS_MAX_NUMBER 64

typedef struct VFSFile {
	char* name;
	char* path;
	UINT64 filePointer;
	UINT64 fileSize;
	struct VirtualFileSystem* filesystem;
}VFSFile;

typedef struct VFSFucntionSet {
	BOOL (*CheckIfFileExists)(struct VirtualFileSystem* filesystem, char* path);

	/* Allocate and initialize VFSFile structure */
	VFSFile* (*OpenFile)(struct VirtualFileSystem* filesystem, char* path);

	/* Deallocate VFSFile structure */
	VOID (*CloseFile)(char* path);

	/* Delete the file without closing the structure */
	UINT64(*DeleteFile)(VFSFile* file);

	/* Delete the file and deallocate VFSFile structure */
	UINT64(*DeleteAndCloseFile)(VFSFile* file);

	/* Create file at given path*/
	UINT64(*CreateFile)(struct VirtualFileSystem* filesystem, char* path);

	/* Create a file for a given VFSFile structure of a file deleted by DeleteFile */
	UINT64(*RecreateDeletedFile)(VFSFile* file);

	/* This does NOT modify the file pointer */
	UINT64(*AppendFile)(VFSFile* file, UINT64 size, VOID* buffer);

	UINT64(*ResizeFile)(VFSFile* file, UINT64 newsize);
	UINT64(*WriteFile)(VFSFile* file, UINT64 size, VOID* buffer);
	UINT64(*ReadFile)(VFSFile* file, UINT64 size, VOID* buffer);

	UINT64(*CreateDirectory)(struct VirtualFileSystem* filesystem, char* path);
	UINT64(*MoveFile)(char* oldPath, char* newPath);
	UINT64(*RenameFile)(VFSFile* file, char* newFileName);
} VFSFunctionSet;

typedef struct VirtualFileSystem {
	IDEDrive* drive;
	UINT64 lbaStart;
	UINT64 sizeInSectors;
	UINT64 allocationUnitSize;
	VFSFunctionSet functions;
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
#define VFS_ERR_ARGUMENT_INVALID				0xFFFF000B

#define VFS_MAX_PATH (4096 - sizeof(MemoryBlock) - 1)

void VFSInit();
UINT32 VFSAddPartition(IDEDrive* drive, UINT64 lbaStart, UINT64 partitionSize, VFSFunctionSet functionSet);
VirtualFileSystem* VFSGetPointerToVFS(unsigned int n);
UINT64 VFSReadSector(VirtualFileSystem*, UINT64 n, BYTE* destination);
UINT64 VFSWriteSector(VirtualFileSystem*, UINT64 n, BYTE* source);

VFSFile* VFSAllocateVFSFile(VFS* filesystem, char* path);
VOID VFSDeallocateVFSFile(VFSFile* vfsFile);

UINT64 FindFirstSlash(char* path);
UINT64 FindLastSlash(char* path);

/* REMEMBER TO RESERVE SPACE FOR NULL TERMINATOR (THIS FUNCTION'S RESULT HAS TO BE INCREMENTED BY 1, IN ORDER TO USE THIS STRING) */
UINT64 GetParentPathLength(char* path);

/* REMEMBER TO RESERVE SPACE FOR NULL TERMINATION */
UINT64 GetParentPath(char* path, char* dst);

UINT64 GetFileNameAndExtensionFromPath(char* path, char* name, char* extension);

#endif