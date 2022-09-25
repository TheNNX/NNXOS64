/*
    This stub initializes paging and relocates rest of the kernel
*/

#include <nnxtype.h>
#include <HAL/paging.h>
#include <SimpleTextIO.h>
#include <../bootloader/bootdata.h>
#include <nnxcfg.h>
#include <HAL/physical_allocator.h>

PDWORD gpdwFramebuffer; PDWORD gpdwFramebufferEnd;
DWORD gdwWidth; DWORD gdwHeight; DWORD gdwPixelsPerScanline;

UINT64 KeEntry();

extern ULONG_PTR gRdspPhysical;

__declspec(noreturn) VOID SetupStack(ULONG_PTR stack, UINT64(*)(PVOID));

static
VOID
RemapSections(
	PSECTION_HEADER SectionHeaders,
	SIZE_T NumberOfSectionHeaders
);

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

	MmReinitPhysAllocator(bootdata->PageFrameDescriptorEntries, bootdata->NumberOfPageFrames);

    /* calculate the new address of our kernel entrypoint */
    mainDelta = (ULONG_PTR)KeEntry - (ULONG_PTR)bootdata->KernelBase;
	mainReloc = (UINT64(*)(VOID*))(KERNEL_DESIRED_LOCATION + mainDelta);
    
    /* map kernel pages */
    NTSTATUS status = PagingInit();

    PagingMapAndInitFramebuffer();

	bootdata->MainKernelModule.SectionHeaders = (PSECTION_HEADER)PagingMapStrcutureToVirtual(
		(ULONG_PTR)bootdata->MainKernelModule.SectionHeaders,
		bootdata->MainKernelModule.NumberOfSectionHeaders * sizeof(SECTION_HEADER), 
		PAGE_WRITE | PAGE_PRESENT
	);

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

	PrintT("Sections: %X\n", bootdata->MainKernelModule.SectionHeaders);
	
	RemapSections(
		bootdata->MainKernelModule.SectionHeaders,
		bootdata->MainKernelModule.NumberOfSectionHeaders
	);
	MiFlagPfnsForRemap();

	SetupStack(newStack, mainReloc);
}

static 
VOID 
RemapSections(
	PSECTION_HEADER SectionHeaders,
	SIZE_T NumberOfSectionHeaders
)
{
	PSECTION_HEADER current;
	INT i, k;
	SIZE_T j;
	BOOLEAN usersection;
	ULONG_PTR currentMapping;
	ULONG_PTR currentPageAddress;
	USHORT newFlags;
	ULONG_PTR physAddress;

	/* remap all sections with name starting with ".user"
	 * to be usermode accessible */
	const char usermodeSectionName[] = ".user";

	for (i = 0; i < NumberOfSectionHeaders; i++)
	{
		current = &SectionHeaders[i];
		
		PrintT("%S: Virtual address: base+%X, size: %i, flags: %b\n", current->Name, 8, current->VirtualAddressRVA, current->VirtualSize, current->Characteristics);
		usersection = TRUE;

		for (k = 0; k < sizeof(usermodeSectionName) - 1; k++)
		{
			if (current->Name[k] != usermodeSectionName[k])
			{
				usersection = FALSE;
			}
		}

		for (j = 0; j < (current->VirtualSize + PAGE_SIZE - 1) / PAGE_SIZE; j++)
		{
			/* KERNEL_DESIRED_LOCATION is our future executable base */
			currentPageAddress = current->VirtualAddressRVA + KERNEL_DESIRED_LOCATION + j * PAGE_SIZE;
			currentMapping = PagingGetCurrentMapping(currentPageAddress);
			physAddress = currentMapping & PAGE_ADDRESS_MASK;
	
			MmMarkPfnAsUsed(PFN_FROM_PA(physAddress));

			/* copy the current flags */
			newFlags = currentMapping & PAGE_FLAGS_MASK;

			/* if this is an usercode section */
			if (usersection)
			{
				newFlags |= PAGE_USER;
			}
			else
			{
				newFlags &= ~PAGE_USER;
			}
			
			/* if this is not a writable section, remove the writable attribute */
			if (!(current->Characteristics & IMAGE_SCN_MEM_WRITE))
			{
				newFlags &= ~PAGE_WRITE;
			}
			else
			{
				newFlags |= PAGE_WRITE;
			}

			PagingMapPage(currentPageAddress, physAddress, newFlags);
		}
	}
}