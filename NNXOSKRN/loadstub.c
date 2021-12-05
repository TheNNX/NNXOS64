/*
    This stub initializes paging and relocates rest of the kernel
*/

#include <nnxtype.h>
#include <memory/paging.h>
#include <memory/MemoryOperations.h>
#include <HAL/IDT.h>
#include <video/SimpleTextIO.h>
#include <../bootloader/bootdata.h>


PDWORD gpdwFramebuffer; PDWORD gpdwFramebufferEnd;
DWORD gdwWidth; DWORD gdwHeight; DWORD gdwPixelsPerScanline;

UINT64 KeEntry(PVOID pRdsp);

extern PBYTE GlobalPhysicalMemoryMap;
extern UINT64 GlobalPhysicalMemoryMapSize;

VOID KeLoadStub(
	BOOTDATA* bootdata
){
    UINT64 i;

    DisableInterrupts();

    gFramebuffer = bootdata->pdwFramebuffer;
    gFramebufferEnd = bootdata->pdwFramebufferEnd;
    gWidth = bootdata->dwWidth;
    gHeight = bootdata->dwHeight;
    gPixelsPerScanline = bootdata->dwPixelsPerScanline;

    bootdata->ExitBootServices(bootdata->pImageHandle, bootdata->mapKey);

    /* calculate the new address of our kernel entrypoint */
    ULONG_PTR mainDelta = (ULONG_PTR)KeEntry - (ULONG_PTR)bootdata->KernelBase;
    UINT64(*mainReloc)(VOID*) = (UINT64(*)(VOID*))(KERNEL_DESIRED_LOCATION + mainDelta);
    
    /* map kernel pages */
    PagingInit(bootdata->pPhysicalMemoryMap, bootdata->qwPhysicalMemoryMapSize);
    PagingMapFramebuffer();

    mainReloc(bootdata->pRdsp);
}