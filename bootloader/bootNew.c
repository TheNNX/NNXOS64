#include <efi.h>
#include <efilib.h>
#include <NNXOSKRN/nnxcfg.h>

EFI_BOOT_SERVICES*  gBootServices;
EFI_HANDLE          gImageHandle;

#define PrintError(status) Print(L"%s():%i status: %r", __FUNCTION__, __LINE__, status)

EFI_STATUS EnumerateDrives()
{
    EFI_HANDLE  *fileSystemHandles;
    UINTN       fileSystemHandleCount;
    UINTN       i;
    EFI_STATUS  status;

    EFI_GUID*   protocolGuid = &gEfiSimpleFileSystemProtocolGuid;

    /* locate all filesystem handles */
    status = gBootServices->LocateHandleBuffer(
        ByProtocol,
        protocolGuid,
        NULL,
        &fileSystemHandleCount,
        &fileSystemHandles
    );

    if (status)
    {
        PrintError(status);
        return status;
    }

    for (i = 0; i < fileSystemHandleCount; i++)
    {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*    protocol;
        EFI_FILE_PROTOCOL*                  rootFile;
        EFI_FILE_SYSTEM_INFO                filesystemInfo;

        status = gBootServices->HandleProtocol(fileSystemHandles[i], protocolGuid, &protocol);
        if (status)
        {
            PrintError(status);
            continue;
        }

        /* open the root file for each volume */
        status = protocol->OpenVolume(protocol, &rootFile);        
        if (status)
        {
            PrintError(status);
            continue;
        }

        /* get filesystem info */
        status = rootFile->GetInfo(rootFile, &gEfiFileSystemInfoGuid, sizeof(EFI_FILE_SYSTEM_INFO), &filesystemInfo);
        if (status)
        {
            PrintError(status);
            continue;
        }

        Print(L"Volume: %ls\n", filesystemInfo.VolumeLabel, filesystemInfo);


        rootFile->Close(rootFile);
    }

    /* LocateHandleBuffer allocates memory with AllocatePool */
    FreePool(fileSystemHandles);
    
    return EFI_SUCCESS;
}

EFI_STATUS efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable)
{
    EFI_STATUS status = EFI_SUCCESS;

    InitializeLib(imageHandle, systemTable);
    
    gBootServices = systemTable->BootServices;
    gImageHandle = imageHandle;

    systemTable->ConOut->ClearScreen(systemTable->ConOut);

    status = EnumerateDrives();
    if (status)
    {
        PrintError(status);
        return status;
    }

    /* boot unsuccessful, return to EFI */
    return EFI_ABORTED;
}