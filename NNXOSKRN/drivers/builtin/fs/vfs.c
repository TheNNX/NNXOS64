#include <vfs.h>
#include <HALX64/include/PCIIDE.h>
#include <nnxalloc.h>
#include <text.h>
#include <rtl.h>

/* TODO: Decouple IDE code form VFS code. Move IDE VFS initialization to 
   apropriate IDE codde. */

VIRTUAL_FILE_SYSTEM virtualFileSystems[VFS_MAX_NUMBER];

VFS_STATUS IdeReadSector(VFS* vfs, SIZE_T sectorIndex, BYTE* destination)
{
    IDE_VFS* ideVfs = vfs->DeviceSpecificData;
    return PciIdeDiskIo(
        ideVfs->Drive, 
        0,
        ideVfs->LbaStart + sectorIndex, 
        1,
        destination);
}

VFS_STATUS IdeWriteSector(VFS* vfs, SIZE_T sectorIndex, BYTE* source)
{
    IDE_VFS* ideVfs = vfs->DeviceSpecificData;
    return PciIdeDiskIo(
        ideVfs->Drive, 
        1, 
        ideVfs->LbaStart + sectorIndex, 
        1, 
        source);
}

void VfsInit()
{
    int i;
    VFS empty = { 0 };

    for (i = 0; i < VFS_MAX_NUMBER; i++)
    {
        virtualFileSystems[i] = empty;
    }
}

SIZE_T VfsRegister(
    PVOID deviceSpecificData,
    VFS_STATUS(*readSector)(VFS*, SIZE_T, PBYTE),
    VFS_STATUS(*writeSector)(VFS*, SIZE_T, PBYTE),
    const VFS_FUNCTION_SET* functions)
{
    SIZE_T i;

    for (i = 0; i < VFS_MAX_NUMBER; i++)
    {
        if (virtualFileSystems[i].DeviceSpecificData == NULL)
        {
            virtualFileSystems[i].DeviceSpecificData = deviceSpecificData;
            virtualFileSystems[i].Functions = *functions;
            virtualFileSystems[i].ReadSector = readSector;
            virtualFileSystems[i].WriteSector = writeSector;
            return i;
        }
    }

    return -1;
}

SIZE_T VfsAddIdePartition(IDE_DRIVE* drive, UINT64 lbaStart, UINT64 partitionSize, const VFS_FUNCTION_SET* functions)
{
    SIZE_T i;

    IDE_VFS* ideSpecific = ExAllocatePool(NonPagedPool, sizeof(IDE_VFS));
    ideSpecific->Drive = drive;
    ideSpecific->LbaStart = lbaStart;
    ideSpecific->SizeInSectors = partitionSize;

    i = VfsRegister(ideSpecific, IdeReadSector, IdeWriteSector, functions);
    if (i == -1)
    {
        ExFreePool(ideSpecific);
    }
    return i;
}

VFS_STATUS VfsReadSector(VIRTUAL_FILE_SYSTEM* vfs, SIZE_T sectorIndex, BYTE* destination)
{
    return vfs->ReadSector(vfs, sectorIndex, destination);
}

VFS_STATUS VfsWriteSector(VIRTUAL_FILE_SYSTEM* vfs, SIZE_T sectorIndex, BYTE* source)
{
    return vfs->WriteSector(vfs, sectorIndex, source);
}

VIRTUAL_FILE_SYSTEM* VfsGetPointerToVfs(SIZE_T n)
{
    if (virtualFileSystems[n].DeviceSpecificData == NULL)
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

    RtlCopyMemory(file->Path, (void*)path, pathLength);

    lastSlash = FindLastSlash(path) + 1;
    fileNameLenght = pathLength - lastSlash;

    file->Name = NNXAllocatorAlloc(fileNameLenght);
    if (!file->Name)
    {
        NNXAllocatorFree(file);
        NNXAllocatorFree(file->Path);
        return 0;
    }

    RtlCopyMemory(file->Name, (void*)(path + lastSlash), fileNameLenght);

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
    RtlCopyMemory(dst, (void*)path, parentPathLength);
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