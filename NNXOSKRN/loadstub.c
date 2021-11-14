/*
    This stub initializes paging and relocates rest of the kernel
*/

#include <nnxtype.h>
#include <memory/paging.h>
#include <memory/MemoryOperations.h>
#include <HAL/IDT.h>
#include <video/SimpleTextIO.h>

UINT64 KeEntry(PVOID data);

/*
int* framebuffer, int* framebufferEnd, UINT32 width, UINT32 height, UINT32 pixelsPerScanline, UINT64(*ExitBootServices)(void*, UINT64), void* imageHandle, UINT64 n,
                UINT8* nnxMMap, UINT64 nnxMMapSize, UINT64 memorySize, VOID* rdsp
*/

PDWORD gpdwFramebuffer; PDWORD gpdwFramebufferEnd;
DWORD gdwWidth; DWORD gdwHeight; DWORD gdwPixelsPerScanline;

extern PBYTE GlobalPhysicalMemoryMap;
extern UINT64 GlobalPhysicalMemoryMapSize;

VOID TestF()
{
    UINT64 i;

    for (i = 0; i < gdwWidth; i++)
    {
        gpdwFramebuffer[i + gdwHeight * gdwPixelsPerScanline / 2] = 0xFF00FF00;
    }
}

__declspec(noreturn) VOID KeLoadStub(
    PDWORD pdwFramebuffer, PDWORD pdwFramebufferEnd,
    DWORD dwWidth, DWORD dwHeight, DWORD dwPixelsPerScanline,
    UINT64(*ExitBootServices)(PVOID, UINT64), PVOID pImageHandle, UINT64 n,
    PBYTE pPhysicalMemoryMap, UINT64 qwPhysicalMemoryMapSize, QWORD qwMemorySize,
    PVOID* pRdsp,
    DWORD dwKernelSize) 
{
    UINT64 i;
 
    DisableInterrupts();

    gFramebuffer = pdwFramebuffer;
    gFramebufferEnd = pdwFramebufferEnd;
    gWidth = dwWidth;
    gHeight = dwHeight;
    gPixelsPerScanline = dwPixelsPerScanline;

    ExitBootServices(pImageHandle, n);
    
    /* calculate the new address of our kernel entrypoint */
    ULONG_PTR mainDelta = (ULONG_PTR)KeEntry - KERNEL_INITIAL_ADDRESS;
    UINT64(*mainReloc)(VOID*) = (UINT64(*)(VOID*))(KERNEL_DESIRED_LOCATION + mainDelta);
    
    /* map kernel pages */
    PagingInit(pPhysicalMemoryMap, qwPhysicalMemoryMapSize);
    PagingMapFramebuffer();

    mainReloc(pRdsp);
}