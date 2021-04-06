#include "vfs.h"
#include "../../HAL/PCI/PCIIDE.h"
#include "../../memory/nnxalloc.h"
#include "../../text.h"
#include "../../memory/MemoryOperations.h"

VirtualFileSystem virtualFileSystems[VFS_MAX_NUMBER];

void VFSInit() {
	VFS empty = { 0 };
	for (int i = 0; i < VFS_MAX_NUMBER; i++)
		virtualFileSystems[i] = empty;
}

UINT32 VFSAddPartition(IDEDrive* drive, UINT64 lbaStart, UINT64 partitionSize, VFSFunctionSet functions) {
	UINT32 found = -1;
	
	for (UINT32 i = 0; (i < VFS_MAX_NUMBER) && (found == -1); i++) {
		if (virtualFileSystems[i].drive == 0) {
			virtualFileSystems[i].drive = drive;
			virtualFileSystems[i].lbaStart = lbaStart;
			virtualFileSystems[i].sizeInSectors = partitionSize;
			virtualFileSystems[i].functions = functions;
			found = i;
			break;
		}
	}

	return found;
}

UINT64 VFSReadSector(VirtualFileSystem* vfs, UINT64 n, BYTE* destination) {
	return PCI_IDE_DiskIO(vfs->drive, 0, vfs->lbaStart + n, 1, destination);
}

UINT64 VFSWriteSector(VirtualFileSystem* vfs, UINT64 n, BYTE* source) {
	return PCI_IDE_DiskIO(vfs->drive, 1, vfs->lbaStart + n, 1, source);
}

VirtualFileSystem* VFSGetPointerToVFS(unsigned int n) {
	return virtualFileSystems + n;
}

UINT64 FindFirstSlash(char* path) {
	UINT64 pathLenght = FindCharacterFirst(path, -1, 0);
	UINT64 forwardSlash = FindCharacterFirst(path, pathLenght, '/');
	UINT64 backSlash = FindCharacterFirst(path, pathLenght, '\\');

	return ((forwardSlash < backSlash) ? (forwardSlash) : (backSlash));
}

UINT64 FindLastSlash(char* path) {
	UINT64 pathLenght = FindCharacterFirst(path, -1, 0);
	UINT64 forwardSlash = FindCharacterLast(path, pathLenght, '/');
	UINT64 backSlash = FindCharacterLast(path, pathLenght, '\\');
	return (((forwardSlash != -1) && ((forwardSlash > backSlash) || (backSlash == -1))) ? (forwardSlash) : (backSlash));
}

VFSFile* VFSAllocateVFSFile(VFS* filesystem, char* path) {
	VFSFile* file = NNXAllocatorAlloc(sizeof(VFSFile));
	if (!file)
		return 0;

	UINT64 pathLength = FindCharacterFirst(path, -1, 0) + 1;

	if (pathLength > VFS_MAX_PATH)
	{
		NNXAllocatorFree(file);
		return 0;
	}

	file->path = NNXAllocatorAlloc(pathLength);

	if (!file->path) {
		NNXAllocatorFree(file);
		return 0;
	}

	MemCopy(file->path, path, pathLength);

	UINT64 lastSlash = FindLastSlash(path) + 1;
	UINT64 fileNameLenght = pathLength - lastSlash;

	file->name = NNXAllocatorAlloc(fileNameLenght);
	if (!file->name) {
		NNXAllocatorFree(file);
		NNXAllocatorFree(file->path);
		return 0;
	}

	MemCopy(file->name, path + lastSlash, fileNameLenght);

	file->filesystem = filesystem;
	file->filePointer = 0;
	file->fileSize = 0;

	return file;
}

VOID VFSDeallocateVFSFile(VFSFile* file) {
	NNXAllocatorFree(file->path);
	NNXAllocatorFree(file->name);
	NNXAllocatorFree(file);
}

UINT64 GetParentPathLength(char* path) {
	UINT64 lastSlash = FindLastSlash(path);
	return lastSlash;
}

UINT64 GetParentPath(char* path, char* dst) {
	UINT64 parentPathLength = GetParentPathLength(path);
	MemCopy(dst, path, parentPathLength);
	dst[parentPathLength] = 0;
	return parentPathLength;
}

UINT64 GetFileNameAndExtensionFromPath(char* path, char* name, char* extension) {
	UINT64 length = FindCharacterFirst(path, -1, 0);
	UINT64 begin = FindLastSlash(path) + 1;
	UINT64 dot = FindCharacterLast(path + begin, length - begin, '.');
	UINT64 filenameEnd = dot == -1 ? length - begin : dot;
	NNXAssertAndStop(filenameEnd <= 8, "Invalid filename");

	for (UINT64 i = 0; i < 8; i++)
		name[i] = ' ';

	for (UINT64 i = 0; i < 3; i++)
		extension[i] = ' ';

	for (UINT64 i = 0; i < filenameEnd; i++) {
		name[i] = (path + begin)[i];
	}
	if (dot != -1) {
		NNXAssertAndStop(length - begin - dot - 1 <= 3, "Invalid extension");
		for (UINT64 i = 0; i < length - begin - dot - 1; i++) {
			extension[i] = (path + begin + dot + 1)[i];
		}
	}
	return begin;
}