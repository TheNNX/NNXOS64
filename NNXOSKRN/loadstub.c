/*
    This stub initializes paging and relocates rest of the kernel
*/

#include <nnxtype.h>
#include <HAL/paging.h>
#include <SimpleTextIO.h>
#include <../bootloader/bootdata.h>
#include <nnxcfg.h>

PDWORD gpdwFramebuffer; PDWORD gpdwFramebufferEnd;
DWORD gdwWidth; DWORD gdwHeight; DWORD gdwPixelsPerScanline;

UINT64 KeEntry();

extern PBYTE GlobalPhysicalMemoryMap;
extern UINT64 GlobalPhysicalMemoryMapSize;
extern ULONG_PTR gRdspPhysical;

__declspec(noreturn) VOID SetupStack(ULONG_PTR stack, UINT64(*)(PVOID));

ULONG_PTR GetStack();

VOID KeLoadStub(
	BOOTDATA* bootdata
){
	ULONG_PTR mainDelta;
	ULONG_PTR newStack;
	ULONG_PTR currentStack;
	ULONG_PTR currentStackPage;
	UINT64(*mainReloc)(VOID*);

	

    gFramebuffer = bootdata->pdwFramebuffer;
    gFramebufferEnd = bootdata->pdwFramebufferEnd;
    gWidth = bootdata->dwWidth;
    gHeight = bootdata->dwHeight;
    gPixelsPerScanline = bootdata->dwPixelsPerScanline;
	gRdspPhysical = (ULONG_PTR)bootdata->pRdsp;

    bootdata->ExitBootServices(bootdata->pImageHandle, bootdata->mapKey);

    /* calculate the new address of our kernel entrypoint */
    mainDelta = (ULONG_PTR)KeEntry - (ULONG_PTR)bootdata->KernelBase;
	mainReloc = (UINT64(*)(VOID*))(KERNEL_DESIRED_LOCATION + mainDelta);
    
    /* map kernel pages */
    PagingInit(bootdata->pPhysicalMemoryMap, bootdata->qwPhysicalMemoryMapSize);
    PagingMapFramebuffer();

	currentStack = GetStack();
	currentStackPage = PAGE_ALIGN(currentStack);

	newStack = (ULONG_PTR)PagingAllocatePageBlockWithPhysicalAddresses(
		STACK_SIZE / PAGE_SIZE,
		PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END,
		PAGE_PRESENT | PAGE_WRITE,
		currentStackPage - STACK_SIZE
	);
	
	newStack += STACK_SIZE;
	newStack -= (currentStack - currentStackPage);

	SetupStack(newStack, mainReloc);
}