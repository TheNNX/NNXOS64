#ifndef NNX_BOOTDATA_HEADER
#define NNX_BOOTDATA_HEADER

#include <nnxtype.h>
#include "../CommonInclude/nnxpe.h"

#pragma pack(push, 1)
typedef struct _LOADED_BOOT_MODULE
{
	PVOID Entrypoint;
	PVOID ImageBase;
	ULONG ImageSize;
	CHAR* Name;
	USHORT OrdinalBase;
	SECTION_HEADER* SectionHeaders;
	SIZE_T NumberOfSectionHeaders;
}LOADED_BOOT_MODULE, * PLOADED_BOOT_MODULE;
#pragma pack(pop)

typedef struct _BOOTDATA
{
	PDWORD pdwFramebuffer;
	PDWORD pdwFramebufferEnd;
	DWORD dwWidth;
	DWORD dwHeight;
	DWORD dwPixelsPerScanline;
	UINT64(*ExitBootServices)(PVOID, UINT64);
	PVOID pImageHandle;
	UINT64 mapKey;
	PBYTE pPhysicalMemoryMap;
	UINT64 qwPhysicalMemoryMapSize;
	PVOID pRdsp;
	DWORD dwKernelSize;
	PVOID KernelBase;
	LOADED_BOOT_MODULE MainKernelModule;
}BOOTDATA, *PBOOTDATA;

#endif
