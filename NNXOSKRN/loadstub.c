/*
    This stub initializes paging and relocates rest of the kernel
*/

#include <nnxtype.h>
#include <paging.h>
#include <SimpleTextIO.h>
#include <../bootloader/bootdata.h>
#include <nnxcfg.h>
#include <physical_allocator.h>
#include <pcr.h>

PDWORD gpdwFramebuffer; PDWORD gpdwFramebufferEnd;
DWORD gdwWidth; DWORD gdwHeight; DWORD gdwPixelsPerScanline;

UINT64 KeEntry();

extern ULONG_PTR gRdspPhysical;

__declspec(noreturn) VOID SetupStack(ULONG_PTR stack, UINT64(*)(PVOID));

static
VOID
BindRelocatedImports(
	PLOADED_BOOT_MODULE Module);

static
VOID
RemapModule(
	PLOADED_BOOT_MODULE Module,
	ULONG_PTR NewBase,
	PULONG_PTR NextSafeBase);

ULONG_PTR GetStack();

VOID KeLoadStub(
	BOOTDATA* bootdata
){
	ULONG_PTR mainDelta;
	ULONG_PTR newStack;
	ULONG_PTR currentStack;
	ULONG_PTR currentStackPage;
	UINT64(*mainReloc)(VOID*);
	PLIST_ENTRY ModuleEntry;
	ULONG_PTR CurrentBase;

	/* Used to temporarily identity-map physical pages allocated by UEFI to 
	 * So exports can be bound to the correct, new virtual addresses. */
	ULONG_PTR MinKernelPhysAddr;
	ULONG_PTR MaxKernelPhysAddr;
	ULONG_PTR CurPhysAddr;

	/* Copy the values from bootloader allocated bootdata, as it will become 
	 * inaccesible once the initial stage of remapping is done. */
    gFramebuffer = bootdata->pdwFramebuffer;
    gFramebufferEnd = bootdata->pdwFramebufferEnd;
    gWidth = bootdata->dwWidth;
    gHeight = bootdata->dwHeight;
    gPixelsPerScanline = bootdata->dwPixelsPerScanline;
	KeKernelPhysicalAddress = bootdata->MainKernelModule->ImageBase;
	gRdspPhysical = (ULONG_PTR)bootdata->pRdsp;
	MinKernelPhysAddr = bootdata->MinKernelPhysAddress;
	MaxKernelPhysAddr = bootdata->MaxKernelPhysAddress;

	/* Initialize the physical page allocator and temporary core PCR. */
	MmReinitPhysAllocator(bootdata->PageFrameDescriptorEntries, bootdata->NumberOfPageFrames);
	HalpInitDummyPcr();
	HalpSetDummyPcr();

    /* Calculate the new address of our kernel entrypoint. */
    mainDelta = (ULONG_PTR)KeEntry - (ULONG_PTR)bootdata->MainKernelModule->ImageBase;
	mainReloc = (UINT64(*)(VOID*))(KERNEL_DESIRED_LOCATION + mainDelta);

    /* Map kernel pages. */
    NTSTATUS status = PagingInit(MinKernelPhysAddr, MaxKernelPhysAddr);
    PagingMapAndInitFramebuffer();
	bootdata->MainKernelModule->SectionHeaders = (PIMAGE_SECTION_HEADER)PagingMapStrcutureToVirtual(
		(ULONG_PTR)bootdata->MainKernelModule->SectionHeaders,
		bootdata->MainKernelModule->NumberOfSectionHeaders * sizeof(IMAGE_SECTION_HEADER), 
		PAGE_WRITE | PAGE_PRESENT);

	/* Remap the stack. */
	currentStack = GetStack();
	currentStackPage = PAGE_ALIGN(currentStack);

	newStack = (ULONG_PTR)PagingAllocatePageBlockWithPhysicalAddresses(
		STACK_SIZE / PAGE_SIZE,
		PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END,
		PAGE_PRESENT | PAGE_WRITE,
		currentStackPage - STACK_SIZE);
	
	newStack += STACK_SIZE;
	newStack -= (currentStack - currentStackPage);

	/* Temporarily identity map addresses corresponding to the original
	 * identity mappings of the bootloader allocated structures, including
	 * the boot module export lists, which are needed to bind the import
	 * entries after the relocation. */
	for (CurPhysAddr = MinKernelPhysAddr;
		CurPhysAddr <= MaxKernelPhysAddr;
		CurPhysAddr += PAGE_SIZE)
	{
		PagingMapPage(CurPhysAddr, CurPhysAddr, PAGE_WRITE | PAGE_PRESENT);
	}

	/* Remap the module sections loaded by the bootloader. */
	ModuleEntry = bootdata->ModuleHead.First;
	CurrentBase = KERNEL_DESIRED_LOCATION;

	while (ModuleEntry != &bootdata->ModuleHead)
	{
		PLOADED_BOOT_MODULE Module =
			CONTAINING_RECORD(ModuleEntry, LOADED_BOOT_MODULE, ListEntry);

		RemapModule(Module, CurrentBase, &CurrentBase);
		ModuleEntry = ModuleEntry->Next;
	}

	/* Bind the import entries. Temporarily disable supervisor readonly page
	 * protection, as the sections are alreay remapped - if import directories 
	 * are in .rdata for example, these pages are marked as readonly. */
	PagingDisableSystemWriteProtection();
	ModuleEntry = bootdata->ModuleHead.First;

	while (ModuleEntry != &bootdata->ModuleHead)
	{
		PLOADED_BOOT_MODULE Module =
			CONTAINING_RECORD(ModuleEntry, LOADED_BOOT_MODULE, ListEntry);

		BindRelocatedImports(Module);
		ModuleEntry = ModuleEntry->Next;
	}
	PagingEnableSystemWriteProtection();

	MiFlagPfnsForRemap();
	SetupStack(newStack, mainReloc);
}

static
VOID
BindRelocatedImports(
	PLOADED_BOOT_MODULE Module)
{
	if (Module->NumberOfDirectoryEntries <= IMAGE_DIRECTORY_ENTRY_IMPORT ||
		Module->DirectoryEntires[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddressRVA
		== NULL)
	{
		return;
	}

	PrintT("Remapping module %X\n", Module);

	PIMAGE_IMPORT_DESCRIPTOR importDesc = 
		(PIMAGE_IMPORT_DESCRIPTOR)
			(Module->DirectoryEntires[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddressRVA +
			Module->ImageBase);

	while (importDesc->NameRVA)
	{
		PrintT("ImportDesc %X %X %s\n", 
			importDesc,
			importDesc->NameRVA + Module->ImageBase, 
			importDesc->NameRVA + Module->ImageBase);

		IMAGE_ILT_ENTRY64* CurrentImport =
			(IMAGE_ILT_ENTRY64*)(importDesc->FirstThunkRVA +
								 Module->ImageBase);

		/* OriginalFirstThunk is repurposed in NNXOS bootmodule loading.
		 * See bootloader/boot.c for more details. */
		IMAGE_ILT_ENTRY64* CurrentExportPreload =
			(IMAGE_ILT_ENTRY64*)(importDesc->OriginalFirstThunk +
							     Module->ImageBase);

		while (CurrentImport->AsNumber)
		{
			PBOOT_MODULE_EXPORT Export = 
				*(PBOOT_MODULE_EXPORT*)CurrentExportPreload;

			PrintT(
				"Import current %X %X %X %X\n", 
				CurrentImport, 
				Export->ExportAddressRva,
				Export->ExportAddress,
				Export);

			*((PULONG_PTR)CurrentImport) = Export->ExportAddress;
			
			CurrentImport++;
			CurrentExportPreload++;
		}

		PrintT("Preincrement %X\n", importDesc);
		importDesc++;
	}
}

static 
VOID 
RemapModule(
	PLOADED_BOOT_MODULE Module,
	ULONG_PTR NewBase,
	PULONG_PTR NextSafeBase)
{
	PIMAGE_SECTION_HEADER current;
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
	PrintT(
		"Remapping module %X from %X to %X (exports: %i)\n", 
		Module->Name,
		Module->ImageBase,
		NewBase,
		Module->NumberOfExports);

	for (i = 0; i < Module->NumberOfSectionHeaders; i++)
	{
		current = &Module->SectionHeaders[i];
		
		PrintT(
			"%S: Virtual address: %X+%X, size: %i, flags: %b\n", 
			current->Name, 8, 
			Module->ImageBase,
			current->VirtualAddressRVA, 
			current->VirtualSize,
			current->Characteristics);

		usersection = TRUE;

		for (k = 0; k < sizeof(usermodeSectionName) - 1; k++)
		{
			if (current->Name[k] != usermodeSectionName[k])
			{
				usersection = FALSE;
			}
		}

		for (j = 0; 
			 j < (current->VirtualSize + PAGE_SIZE - 1) / PAGE_SIZE; 
			 j++)
		{
			/* KERNEL_DESIRED_LOCATION is our future executable base */
			currentPageAddress = 
				current->VirtualAddressRVA + 
				NewBase +
				j * PAGE_SIZE;

			if (currentPageAddress + PAGE_SIZE > *NextSafeBase)
			{
				*NextSafeBase = currentPageAddress + PAGE_SIZE;
			}

			physAddress = 
				Module->ImageBase +
				current->VirtualAddressRVA + 
				j * PAGE_SIZE;

			currentMapping = PagingGetCurrentMapping(currentPageAddress);
			physAddress |= currentMapping & PAGE_FLAGS_MASK;
	
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

	Module->OriginalBase = Module->ImageBase;
	Module->ImageBase = NewBase;
	for (i = 0; i < Module->NumberOfExports; i++)
	{
		PBOOT_MODULE_EXPORT Export = &Module->Exports[i];		
		Export->ExportAddress = Export->ExportAddressRva + Module->ImageBase;
	}
}