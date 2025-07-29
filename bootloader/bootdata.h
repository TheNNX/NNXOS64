#ifndef NNX_BOOTDATA_HEADER
#define NNX_BOOTDATA_HEADER

#include <nnxtype.h>
#include "../CommonInclude/nnxpe.h"
#include <ntlist.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#pragma pack(push, 1)

typedef struct _BOOT_MODULE_EXPORT
{
    ULONG_PTR     ExportAddress;
    PCHAR         ExportName;
    RVA           ExportAddressRva;
    RVA           ExportNameRva;
}BOOT_MODULE_EXPORT, *PBOOT_MODULE_EXPORT;

typedef struct _LOADED_BOOT_MODULE
{
    UINT64                  Entrypoint;
    ULONG_PTR               ImageBase;
    ULONG                   ImageSize;
    CHAR*                   Name;
    USHORT                  OrdinalBase;
    PIMAGE_SECTION_HEADER   SectionHeaders;
    SIZE_T                  NumberOfSectionHeaders;
    LIST_ENTRY              ListEntry;
    PBOOT_MODULE_EXPORT     Exports;
    SIZE_T                  NumberOfExports;
    ULONG_PTR               OriginalBase;
    INT                     NumberOfDirectoryEntries;
    IMAGE_DATA_DIRECTORY    DirectoryEntires[16];
    ULONG_PTR               PreferredBase;
} LOADED_BOOT_MODULE, * PLOADED_BOOT_MODULE;
#pragma pack(pop)

typedef struct _BOOTDATA
{
    PDWORD                   pdwFramebuffer;
    PDWORD                   pdwFramebufferEnd;
    DWORD                    dwWidth;
    DWORD                    dwHeight;
    DWORD                    dwPixelsPerScanline;
    PVOID                    pImageHandle;
    struct _MMPFN_ENTRY*     PageFrameDescriptorEntries;
    SIZE_T                   NumberOfPageFrames;
    PVOID                    pRdsp;
    DWORD                    dwKernelSize;
    LOADED_BOOT_MODULE*      MainKernelModule;
    LIST_ENTRY               ModuleHead;
    ULONG_PTR                MinKernelPhysAddress;
    ULONG_PTR                MaxKernelPhysAddress;
    LIST_ENTRY               PreloadedFiles;
} BOOTDATA, *PBOOTDATA;

typedef struct _PRELOADED_FILE
{
    LIST_ENTRY Entry;
    WCHAR      Name[MAX_PATH];
    SIZE_T     Filesize;
    PVOID      Data;
} PRELOADED_FILE, *PPRELOADED_FILE;

#endif
