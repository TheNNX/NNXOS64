#include <nnxtype.h>
#include <bootloader/bootdata.h>
#include <preloaded.h>
#include <rtl.h>
#include <vfs.h>

/* TODO: synchronization! */

PLIST_ENTRY KePreloadedHeadPtr;

static
VFS_STATUS
VfsAccessInvalid(
    VFS* vfs,
    SIZE_T s,
    PBYTE b)
{
    return VFS_ERR_INACCESSIBLE;
}

static
BOOLEAN
GetNameMatches(
    const WCHAR* wideName,
    const char* asciiName)
{
    while (*wideName && *asciiName && *wideName == *asciiName)
    {
        wideName++;
        asciiName++;
    }

    return ('\0' == *wideName && '\0' == *asciiName);
}

static
PPRELOADED_FILE
PreloadedGetFile(
    const /* TODO */ char* path)
{
    PLIST_ENTRY pCurrent;
    PPRELOADED_FILE pPreloaded;
    
    pCurrent = KePreloadedHeadPtr->First;

    while (pCurrent != KePreloadedHeadPtr)
    {
        pPreloaded = CONTAINING_RECORD(pCurrent, PRELOADED_FILE, Entry);

        if (GetNameMatches(pPreloaded->Name, path))
        {
            return pPreloaded;
        }

        pCurrent = pCurrent->Next;
    }

    PrintT("[%s] Warning: %s not found\n", __FUNCTION__, path);
    return NULL;
}

BOOL PreloadedCheckIfFileExists(struct VIRTUAL_FILE_SYSTEM* filesystem, const char* path)
{
    return PreloadedGetFile(path) != NULL;
}

VFS_FILE* PreloadedOpenFile(struct VIRTUAL_FILE_SYSTEM* vfs, const char* path)
{
    PPRELOADED_FILE pPreloaded;

    pPreloaded = PreloadedGetFile(path);

    VFS_FILE* file = VfsAllocateVfsFile(vfs, path);
    file->FileSize = pPreloaded->Filesize;

    return file;
}

VFS_FILE* PreloadedOpenOrCreateFile(struct VIRTUAL_FILE_SYSTEM* filesystem, const char* path)
{
    return PreloadedOpenFile(filesystem, path);
}

VOID PreloadedCloseFile(VFS_FILE* file)
{
    VfsDeallocateVfsFile(file);
}

VFS_STATUS PreloadedReadFile(VFS_FILE* file, SIZE_T size, VOID* buffer)
{
    PPRELOADED_FILE pPreloaded;
    /* TODO: range checks */

    pPreloaded = PreloadedGetFile(file->Path);
    RtlCopyMemory(buffer, (PBYTE)pPreloaded->Data + file->FilePointer, size);

    file->FilePointer += size;
    return STATUS_SUCCESS;
}

VFS_STATUS PreloadedDeleteFile(VFS_FILE* file)
{
    return VFS_ERR_READONLY;
}

VFS_STATUS PreloadedDeleteAndCloseFile(VFS_FILE* file)
{
    return VFS_ERR_READONLY;
}

VFS_STATUS PreloadedCreateFile(struct VIRTUAL_FILE_SYSTEM* filesystem, const char* path)
{
    return VFS_ERR_READONLY;
}

VFS_STATUS PreloadedRecreateDeletedFile(VFS_FILE* file)
{
    return VFS_ERR_READONLY;
}

VFS_STATUS PreloadedAppendFile(VFS_FILE* file, SIZE_T size, VOID* buffer)
{
    return VFS_ERR_READONLY;
}

VFS_STATUS PreloadedResizeFile(VFS_FILE* file, SIZE_T newsize)
{
    return VFS_ERR_READONLY;
}

VFS_STATUS PreloadedWriteFile(VFS_FILE* file, SIZE_T size, VOID* buffer)
{
    return VFS_ERR_READONLY;
}

VFS_STATUS PreloadedCreateDirectory(struct VIRTUAL_FILE_SYSTEM* filesystem, const char* path)
{
    return VFS_ERR_READONLY;
}

VFS_STATUS PreloadedMoveFile(const char* oldPath, const char* newPath)
{
    return VFS_ERR_READONLY;
}

VFS_STATUS PreloadedRenameFile(VFS_FILE* file, const char* newFileName)
{
    return VFS_ERR_READONLY;
}

NTSTATUS
NTAPI
KeInitPreloadedFilesystem()
{
    VFS_FUNCTION_SET functionSet;

    if (KePreloadedHeadPtr == NULL)
    {
        return STATUS_NOT_SUPPORTED;
    }

    functionSet.AppendFile = PreloadedAppendFile;
    functionSet.CheckIfFileExists = PreloadedCheckIfFileExists;
    functionSet.CloseFile = PreloadedCloseFile;
    functionSet.CreateDirectory = PreloadedCreateDirectory;
    functionSet.CreateFile = PreloadedCreateFile;
    functionSet.DeleteAndCloseFile = PreloadedDeleteAndCloseFile;
    functionSet.DeleteFile = PreloadedDeleteFile;
    functionSet.MoveFile = PreloadedMoveFile;
    functionSet.OpenFile = PreloadedOpenFile;
    functionSet.OpenOrCreateFile = PreloadedOpenOrCreateFile;
    functionSet.ReadFile = PreloadedReadFile;
    functionSet.RecreateDeletedFile = PreloadedRecreateDeletedFile;
    functionSet.RenameFile = PreloadedRenameFile;
    functionSet.ResizeFile = PreloadedResizeFile;
    functionSet.WriteFile = PreloadedWriteFile;

    VfsRegister((PVOID)-1, VfsAccessInvalid, VfsAccessInvalid, &functionSet);
    
    return STATUS_SUCCESS;
}