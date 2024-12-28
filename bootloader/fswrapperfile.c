#include "fswrapper.h"
#include <efilib.h>

static
EFI_STATUS 
FsWrapperFileClose(
    FILE_WRAPPER_HANDLE Handle);

static
EFI_STATUS
FsWrapperFileOpen(
    FILE_WRAPPER_HANDLE self,
    FILE_WRAPPER_HANDLE* newHandle,
    CHAR16* filename,
    UINT64 openMode,
    UINT64 attributes);

static
EFI_STATUS
FsWrapperFileRead(
    FILE_WRAPPER_HANDLE self,
    UINTN* bufferSize,
    void* buffer);

static
EFI_STATUS
FsWrapperFileSetPosition(
    FILE_WRAPPER_HANDLE self,
    UINT64 position);

static
EFI_STATUS
FsWrapperFileGetInfo(
    FILE_WRAPPER_HANDLE self,
    EFI_GUID* infoType,
    UINTN* bufferSize,
    void* buffer);

static
void 
FsWrapperFilePopulate(
    EFI_FILE_HANDLE efiFile, 
    FILE_WRAPPER_HANDLE dst)
{
    dst->Close = FsWrapperFileClose;
    dst->Open = FsWrapperFileOpen;
    dst->Read = FsWrapperFileRead;
    dst->SetPosition = FsWrapperFileSetPosition;
    dst->GetInfo = FsWrapperFileGetInfo;
    dst->FileHandle = efiFile;
}

static
EFI_STATUS 
FsWrapperFileClose(
    FILE_WRAPPER_HANDLE Handle)
{
    EFI_STATUS status;

    if (Handle->FileHandle != NULL)
    {
        status = Handle->FileHandle->Close(Handle->FileHandle);

        if (EFI_ERROR(status))
        {
            return status;
        }

        Handle->FileHandle = NULL;
    }

    status = gBS->FreePool(Handle);
    return status;
}

static
EFI_STATUS 
FsWrapperFileOpen(
    FILE_WRAPPER_HANDLE self,
    FILE_WRAPPER_HANDLE* newHandle,
    CHAR16* filename,
    UINT64 openMode,
    UINT64 attributes)
{
    FILE_WRAPPER_HANDLE result;
    EFI_STATUS status;

    EFI_FILE_HANDLE efiFile;

    status = self->FileHandle->Open(self->FileHandle, &efiFile, filename, openMode, attributes);
    if (EFI_ERROR(status))
    {
        return status;
    }

    status = gBS->AllocatePool(EfiLoaderData, sizeof(*result), &result);
    if (EFI_ERROR(status))
    {
        efiFile->Close(efiFile);
        return status;
    }

    FsWrapperFilePopulate(efiFile, result);
    *newHandle = result;
    return EFI_SUCCESS;
}

static
EFI_STATUS
FsWrapperFileRead(
    FILE_WRAPPER_HANDLE self,
    UINTN* bufferSize,
    void* buffer)
{
    return self->FileHandle->Read(self->FileHandle, bufferSize, buffer);
}

static
EFI_STATUS
FsWrapperFileSetPosition(
    FILE_WRAPPER_HANDLE self,
    UINT64 position)
{
    return self->FileHandle->SetPosition(self->FileHandle, position);
}

static
EFI_STATUS
FsWrapperFileGetInfo(
    FILE_WRAPPER_HANDLE self,
    EFI_GUID* infoType,
    UINTN* bufferSize,
    void* buffer)
{
    return self->FileHandle->GetInfo(self->FileHandle, infoType, bufferSize, buffer);
}

EFI_STATUS 
FsWrapperOpenDriveRoot(
    EFI_HANDLE imageHandle, 
    FILE_WRAPPER_HANDLE* pOut)
{
    EFI_LOADED_IMAGE_PROTOCOL* loadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystem;
    EFI_STATUS status;
    EFI_FILE_HANDLE efiFile;
    
    FILE_WRAPPER_HANDLE result;

    status = gBS->AllocatePool(EfiLoaderData, sizeof(*result), &result);
    if (EFI_ERROR(status))
    {
        return status;
    }

    status = gBS->HandleProtocol(
        imageHandle,
        &gEfiLoadedImageProtocolGuid,
        &loadedImage);
    if (EFI_ERROR(status))
    {
        gBS->FreePool(result);
        return status;
    }

    status = gBS->HandleProtocol(
        loadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        &filesystem);
    if (EFI_ERROR(status))
    {
        gBS->FreePool(result);
        return status;
    }

    status = filesystem->OpenVolume(
        filesystem,
        &efiFile);
    if (EFI_ERROR(status))
    {
        gBS->FreePool(result);
        return status;
    }

    FsWrapperFilePopulate(efiFile, result);
    *pOut = result;
    
    return EFI_SUCCESS;
}