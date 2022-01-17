#ifndef NNX_VFS_HEADER
#define NNX_VFS_HEADER

#include <HAL/PCI/PCI.h>
#include <nnxalloc.h>
#include <HAL/spinlock.h>
#define VFS_MAX_NUMBER 64

#ifdef __cplusplus
extern "C" {
#endif

	typedef ULONG VFS_STATUS;

	typedef struct VFS_FILE
	{
		char* Name;
		char* Path;
		UINT64 FilePointer;
		UINT64 FileSize;
		struct VIRTUAL_FILE_SYSTEM* Filesystem;
	}VFS_FILE;

	typedef struct VFS_FUNCTION_SET
	{
		BOOL(*CheckIfFileExists)(struct VIRTUAL_FILE_SYSTEM* filesystem, const char* path);

		/* Allocate and initialize VFS_FILE structure */
		VFS_FILE* (*OpenFile)(struct VIRTUAL_FILE_SYSTEM* filesystem, const char* path);

		/* Create if file does not exist */
		VFS_FILE* (*OpenOrCreateFile)(struct VIRTUAL_FILE_SYSTEM* filesystem, const char* path);

		/* Deallocate VFS_FILE structure */
		VOID(*CloseFile)(VFS_FILE* file);

		/* Delete the file without closing the structure */
		VFS_STATUS(*DeleteFile)(VFS_FILE* file);

		/* Delete the file and deallocate VFS_FILE structure */
		VFS_STATUS(*DeleteAndCloseFile)(VFS_FILE* file);

		/* Create file at given Path*/
		VFS_STATUS(*CreateFile)(struct VIRTUAL_FILE_SYSTEM* filesystem, const char* path);

		/* Create a file for a given VFS_FILE structure of a file deleted by DeleteFile */
		VFS_STATUS(*RecreateDeletedFile)(VFS_FILE* file);

		/* This does NOT modify the file pointer */
		VFS_STATUS(*AppendFile)(VFS_FILE* file, SIZE_T size, VOID* buffer);

		VFS_STATUS(*ResizeFile)(VFS_FILE* file, SIZE_T newsize);
		VFS_STATUS(*WriteFile)(VFS_FILE* file, SIZE_T size, VOID* buffer);
		VFS_STATUS(*ReadFile)(VFS_FILE* file, SIZE_T size, VOID* buffer);

		VFS_STATUS(*CreateDirectory)(struct VIRTUAL_FILE_SYSTEM* filesystem, const char* path);
		VFS_STATUS(*MoveFile)(const char* oldPath, const char* newPath);
		VFS_STATUS(*RenameFile)(VFS_FILE* file, const char* newFileName);
	} VFS_FUNCTION_SET;

	typedef struct _IDE_DRIVE IDE_DRIVE;

	typedef struct VIRTUAL_FILE_SYSTEM
	{
		IDE_DRIVE* Drive;
		UINT64 LbaStart;
		UINT64 SizeInSectors;
		UINT64 AllocationUnitSize;
		VFS_FUNCTION_SET Functions;
		VOID* FilesystemSpecificData;
		KSPIN_LOCK DeviceSpinlock;
	}VIRTUAL_FILE_SYSTEM, VFS;


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

#define VFS_MAX_PATH (4096 - sizeof(MEMORY_BLOCK) - 1)

	void VfsInit();
	SIZE_T VfsAddPartition(IDE_DRIVE* drive, UINT64 lbaStart, UINT64 partitionSize, VFS_FUNCTION_SET functionSet);
	VIRTUAL_FILE_SYSTEM* VfsGetPointerToVfs(SIZE_T n);
	VIRTUAL_FILE_SYSTEM* VfsGetSystemVfs();
	VFS_STATUS VfsReadSector(VIRTUAL_FILE_SYSTEM*, SIZE_T sectorIndex, BYTE* destination);
	VFS_STATUS VfsWriteSector(VIRTUAL_FILE_SYSTEM*, SIZE_T sectorIndex, BYTE* source);
	VFS_FILE* VfsAllocateVfsFile(VFS* filesystem, const char* path);
	VOID VfsDeallocateVfsFile(VFS_FILE* vfsFile);

	SIZE_T FindFirstSlash(const char * path);
	SIZE_T FindLastSlash(const char * path);

	/* REMEMBER TO RESERVE SPACE FOR NULL TERMINATOR (THIS FUNCTION'S RESULT HAS TO BE INCREMENTED BY 1, IN ORDER TO USE THIS STRING) */
	SIZE_T GetParentPathLength(const char * path);

	/* REMEMBER TO RESERVE SPACE FOR NULL TERMINATION */
	SIZE_T GetParentPath(const char* path, char* dst);

	SIZE_T GetFileNameAndExtensionFromPath(const char * path, char* name, char* extension);

#ifdef __cplusplus
}
#endif

#endif