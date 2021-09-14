#include "vfs.h"
#include "../../HAL/PCI/PCIIDE.h"
#include "../../memory/nnxalloc.h"
#include "../../text.h"
#include "../../memory/MemoryOperations.h"

VIRTUAL_FILE_SYSTEM virtualFileSystems[VFS_MAX_NUMBER];

void VfsInit()
{
	VFS empty = { 0 };
	for (int i = 0; i < VFS_MAX_NUMBER; i++)
		virtualFileSystems[i] = empty;
}

UINT32 VfsAddPartition(IDE_DRIVE* drive, UINT64 lbaStart, UINT64 partitionSize, VFS_FUNCTION_SET functions)
{
	UINT32 found = -1;

	for (UINT32 i = 0; (i < VFS_MAX_NUMBER) && (found == -1); i++)
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

UINT64 VfsReadSector(VIRTUAL_FILE_SYSTEM* vfs, UINT64 n, BYTE* destination)
{
	return PciIdeDiskIo(vfs->Drive, 0, vfs->LbaStart + n, 1, destination);
}

UINT64 VfsWriteSector(VIRTUAL_FILE_SYSTEM* vfs, UINT64 n, BYTE* source)
{
	return PciIdeDiskIo(vfs->Drive, 1, vfs->LbaStart + n, 1, source);
}

VIRTUAL_FILE_SYSTEM* VfsGetPointerToVfs(unsigned int n)
{
	return virtualFileSystems + n;
}

VIRTUAL_FILE_SYSTEM* VfsGetSystemVfs()
{
	// TODO
	return VfsGetPointerToVfs(0);
}

UINT64 FindFirstSlash(const char * path)
{
	UINT64 pathLenght = FindCharacterFirst(path, -1, 0);
	UINT64 forwardSlash = FindCharacterFirst(path, pathLenght, '/');
	UINT64 backSlash = FindCharacterFirst(path, pathLenght, '\\');

	return ((forwardSlash < backSlash) ? (forwardSlash) : (backSlash));
}

UINT64 FindLastSlash(const char * path)
{
	UINT64 pathLenght = FindCharacterFirst(path, -1, 0);
	UINT64 forwardSlash = FindCharacterLast(path, pathLenght, '/');
	UINT64 backSlash = FindCharacterLast(path, pathLenght, '\\');
	return (((forwardSlash != -1) && ((forwardSlash > backSlash) || (backSlash == -1))) ? (forwardSlash) : (backSlash));
}

VFS_FILE* VfsAllocateVfsFile(VFS* filesystem, char* path)
{
	VFS_FILE* file = NNXAllocatorAlloc(sizeof(VFS_FILE));
	if (!file)
		return 0;

	UINT64 pathLength = FindCharacterFirst(path, -1, 0) + 1;

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

	MemCopy(file->Path, path, pathLength);

	UINT64 lastSlash = FindLastSlash(path) + 1;
	UINT64 fileNameLenght = pathLength - lastSlash;

	file->Name = NNXAllocatorAlloc(fileNameLenght);
	if (!file->Name)
	{
		NNXAllocatorFree(file);
		NNXAllocatorFree(file->Path);
		return 0;
	}

	MemCopy(file->Name, path + lastSlash, fileNameLenght);

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

UINT64 GetParentPathLength(const char * path)
{
	UINT64 lastSlash = FindLastSlash(path);
	return lastSlash;
}

UINT64 GetParentPath(char* path, char* dst)
{
	UINT64 parentPathLength = GetParentPathLength(path);
	MemCopy(dst, path, parentPathLength);
	dst[parentPathLength] = 0;
	return parentPathLength;
}

UINT64 GetFileNameAndExtensionFromPath(const char * path, char* name, char* extension)
{
	UINT64 length = FindCharacterFirst(path, -1, 0);
	UINT64 begin = FindLastSlash(path) + 1;
	UINT64 dot = FindCharacterLast(path + begin, length - begin, '.');
	UINT64 filenameEnd = dot == -1 ? length - begin : dot;
	NNXAssertAndStop(filenameEnd <= 8, "Invalid filename");

	for (UINT64 i = 0; i < 8; i++)
		name[i] = ' ';

	for (UINT64 i = 0; i < 3; i++)
		extension[i] = ' ';

	for (UINT64 i = 0; i < filenameEnd; i++)
	{
		name[i] = (path + begin)[i];
	}
	if (dot != -1)
	{
		NNXAssertAndStop(length - begin - dot - 1 <= 3, "Invalid extension");
		for (UINT64 i = 0; i < length - begin - dot - 1; i++)
		{
			extension[i] = (path + begin + dot + 1)[i];
		}
	}
	return begin;
}