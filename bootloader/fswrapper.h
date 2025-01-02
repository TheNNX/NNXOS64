#ifndef NNX_FSWRAPPER_H
#define NNX_FSWRAPPER_H

#include <efi.h>

#ifndef __INTELLISENSE__
#ifdef __cplusplus
extern "C" {
#endif
#endif

typedef struct _FILE_WRAPPER FILE_WRAPPER, *FILE_WRAPPER_HANDLE;
struct _FILE_WRAPPER
{
    VOID* pWrapperContext;

    EFI_STATUS(*Close)(
        FILE_WRAPPER_HANDLE self);

    EFI_STATUS(*Open)(
        FILE_WRAPPER_HANDLE self,
        FILE_WRAPPER_HANDLE* newHandle,
        CHAR16* filename,
        UINT64 openMode,
        UINT64 attributes);

    EFI_STATUS(*Read)(
        FILE_WRAPPER_HANDLE self, 
        UINTN* bufferSize, 
        void* buffer);

    EFI_STATUS(*SetPosition)(
        FILE_WRAPPER_HANDLE self,
        UINT64 position);

    EFI_STATUS(*GetInfo)(
        FILE_WRAPPER_HANDLE self,
        EFI_GUID* infoType,
        UINTN* bufferSize,
        void* buffer);
};

EFI_STATUS 
FsWrapperOpenDriveRoot(
    EFI_HANDLE imageHandle, 
    FILE_WRAPPER_HANDLE *pOut);

EFI_STATUS 
FsWrapperOpenNetworkRoot(
    FILE_WRAPPER_HANDLE* pOut, 
    const CHAR16* address);

#ifndef __INTELLISENSE__
#ifdef __cplusplus
}
#endif
#endif
#endif
