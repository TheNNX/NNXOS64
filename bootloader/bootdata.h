#ifndef NNX_BOOTDATA_HEADER
#define NNX_BOOTDATA_HEADER

#include <nnxtype.h>

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
}BOOTDATA;

#endif // !NNX_BOOTDATA_HEADER
