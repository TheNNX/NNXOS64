#include "vfs.h"
#include <HAL/PCI/PCIIDE.h>
#include <nnxalloc.h>
#include <text.h>
#include <MemoryOperations.h>

VIRTUAL_FILE_SYSTEM virtualFileSystems[VFS_MAX_NUMBER];

void VfsInit()
{
	int i;
	VFS empty = { 0 };
	
	for (i = 0; i < VFS_MAX_NUMBER; i++)
		virtualFileSystems[i] = empty;
}

SIZE_T VfsAddPartition(IDE_DRIVE* drive, UINT64 lbaStart, UINT64 partitionSize, VFS_FUNCTION_SET functions)
{
	SIZE_T found = -1;
	SIZE_T i;

	for (i = 0; (i < VFS_MAX_NUMBER) && (found == -1); i++)
	{
		if (virtualFileSystems[i].Drive == 0)
		{
			virtualFileSystems[i].Drive = drive;
			virtualFileSystems[i].LbaStart = lbaStart;
			virtualFileSystems[i].SizeInSectors = partitionSize;
			virtualFileSystems[i].Functions = functions;
			found = i;
			break;
		}
	}

	return found;
}

VFS_STATUS VfsReadSector(VIRTUAL_FILE_SYSTEM* vfs, SIZE_T sectorIndex, BYTE* destination)
{
	return PciIdeDiskIo(vfs->Drive, 0, vfs->LbaStart + sectorIndex, 1, destination);
}

VFS_STATUS VfsWriteSector(VIRTUAL_FILE_SYSTEM* vfs, SIZE_T sectorIndex, BYTE* source)
{
	return PciIdeDiskIo(vfs->Drive, 1, vfs->LbaStart + sectorIndex, 1, source);
}

VIRTUAL_FILE_SYSTEM* VfsGetPointerToVfs(SIZE_T n)
{
	if (virtualFileSystems[n].Drive == 0)
	{
		return NULL;
	}
	return &virtualFileSystems[n];
}

VIRTUAL_FILE_SYSTEM* VfsGetSystemVfs()
{
	// TODO
	return VfsGetPointerToVfs(0);
}

SIZE_T FindFirstSlash(const char * path)
{
	SIZE_T pathLenght = FindCharacterFirst(path, -1, 0);
	SIZE_T forwardSlash = FindCharacterFirst(path, pathLenght, '/');
	SIZE_T backSlash = FindCharacterFirst(path, pathLenght, '\\');

	return ((forwardSlash < backSlash) ? (forwardSlash) : (backSlash));
}

SIZE_T FindLastSlash(const char * path)
{
	SIZE_T pathLenght = FindCharacterFirst(path, -1, 0);
	SIZE_T forwardSlash = FindCharacterLast(path, pathLenght, '/');
	SIZE_T backSlash = FindCharacterLast(path, pathLenght, '\\');

	return (((forwardSlash != -1) && ((forwardSlash > backSlash) || (backSlash == -1))) ? (forwardSlash) : (backSlash));
}

VFS_FILE* VfsAllocateVfsFile(VFS* filesystem, const char* path)
{
	VFS_FILE* file = NNXAllocatorAlloc(sizeof(VFS_FILE));
	SIZE_T pathLength;
	SIZE_T lastSlash;
	SIZE_T fileNameLenght;
	
	if (!file)
		return 0;

	pathLength = FindCharacterFirst(path, -1, 0) + 1;

	if (pathLength > VFS_MAX_PATH)
	{
		NNXAllocatorFree(file);
		return 0;
	}

	file->Path = NNXAllocatorAlloc(pathLength);

	if (!file->Path)
	{
		NNXAllocatorFree(file);
		return 0;
	}

	MemCopy(file->Path, (void*)path, pathLength);

	lastSlash = FindLastSlash(path) + 1;
	fileNameLenght = pathLength - lastSlash;

	file->Name = NNXAllocatorAlloc(fileNameLenght);
	if (!file->Name)
	{
		NNXAllocatorFree(file);
		NNXAllocatorFree(file->Path);
		return 0;
	}

	MemCopy(file->Name, (void*)(path + lastSlash), fileNameLenght);

	file->Filesystem = filesystem;
	file->FilePointer = 0;
	file->FileSize = 0;

	return file;
}

VOID VfsDeallocateVfsFile(VFS_FILE* file)
{
	NNXAllocatorFree(file->Path);
	NNXAllocatorFree(file->Name);
	NNXAllocatorFree(file);
}

SIZE_T GetParentPathLength(const char * path)
{
	SIZE_T lastSlash = FindLastSlash(path);
	return lastSlash;
}

SIZE_T GetParentPath(const char* path, char* dst)
{
	SIZE_T parentPathLength = GetParentPathLength(path);
	MemCopy(dst, (void*)path, parentPathLength);
	dst[parentPathLength] = 0;
	return parentPathLength;
}

SIZE_T GetFileNameAndExtensionFromPath(const char * path, char* name, char* extension)
{
	SIZE_T length = FindCharacterFirst(path, -1, 0);
	SIZE_T begin = FindLastSlash(path) + 1;
	SIZE_T dot = FindCharacterLast(path + begin, length - begin, '.');
	SIZE_T filenameEnd = dot == -1 ? length - begin : dot;
	SIZE_T i, j;

	for (i = 0; i < 8; i++)
		name[i] = ' ';

	for (i = 0; i < 3; i++)
		extension[i] = ' ';

	for (i = 0; i < filenameEnd; i++)
	{
		name[i] = (path + begin)[i];
	}
	if (dot != -1)
	{
		for (j = 0; j < length - begin - dot - 1; j++)
		{
			extension[j] = (path + begin + dot + 1)[j];
		}
	}
	return begin;
}