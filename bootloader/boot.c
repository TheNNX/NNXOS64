/* TODO: separate out PE32 loading */
/* TODO: finish import resolving */

#include <efi.h>
#include <efilib.h>
#include <nnxtype.h>
#include <NNXOSKRN/nnxcfg.h>
#include <nnxpe.h>
#include <bootdata.h>
#include <NNXOSKRN/memory/physical_allocator.h>

#define ALLOC(x) AllocateZeroPool(x)
#define DEALLOC(x) FreePool(x)

#include <NNXOSKRN/klist.h>

/* kinda ugly, but gets the job done (and is far prettier than the mess that it was before) */
#define return_if_error_a(status, d) if (EFI_ERROR(status)) { if(d) Print(L"Line: %d Status: %r\n", __LINE__, status); return status; }
#define return_if_error(status) return_if_error_a(status, 0)
#define return_if_error_debug(status) return_if_error_a(status, 1)

const CHAR16 *wszKernelPath = L"efi\\boot\\NNXOSKRN.exe";

EFI_BOOT_SERVICES* gBootServices;

typedef struct _MODULE_EXPORT
{
	char *Name;
	PVOID Address;
}MODULE_EXPORT, *PMODULE_EXPORT;

typedef struct _LOADED_MODULE
{
	KLINKED_LIST Exports;
	PVOID Entrypoint;
	PVOID ImageBase;
	ULONG ImageSize;
	CHAR* Name;
	USHORT OrdinalBase;
}LOADED_MODULE, *PLOADED_MODULE;

VOID DestroyModuleExport(PVOID exportPointer)
{
	MODULE_EXPORT* export = (MODULE_EXPORT*) exportPointer;
	FreePool(export);
}

VOID DestroyLoadedModule(PVOID modulePointer)
{
	LOADED_MODULE* module = (LOADED_MODULE*) modulePointer;
	ClearListAndDestroyValues(&module->Exports, DestroyModuleExport);
	FreePool(module);
}

KLINKED_LIST LoadedModules;

VOID PrintExports()
{
	PKLINKED_LIST c = LoadedModules.Next;
	while (c)
	{
		LOADED_MODULE* module = ((LOADED_MODULE*) c->Value);
		Print(L"%X\n", module->ImageBase);
		c = c->Next;

		PKLINKED_LIST e = module->Exports.Next;

		while (e)
		{
			Print(L"   %a=%X\n", ((MODULE_EXPORT*) e->Value)->Name, ((MODULE_EXPORT*) e->Value)->Address);
			e = e->Next;
		}
	}
}

BOOL CompareModuleName(PVOID a, PVOID b)
{
	CHAR* name = b;
	CHAR* moduleName = ((LOADED_MODULE*) a)->Name;

	return strcmpa(name, moduleName) == 0;
}

EFI_STATUS TryToLoadModule(CHAR* name)
{
	return EFI_UNSUPPORTED;
}

EFI_STATUS HandleImportDirectory(LOADED_MODULE* module, IMAGE_IMPORT_DIRECTORY_ENTRY* importDirectoryEntry)
{
	EFI_STATUS status;
	PVOID imageBase = module->ImageBase;

	IMAGE_IMPORT_DESCRIPTOR* current = importDirectoryEntry->Entries;

	while (current->NameRVA != 0)
	{
		CHAR* name = (CHAR*)((ULONG_PTR)current->NameRVA + (ULONG_PTR)imageBase);
		IMAGE_ILT_ENTRY64* imports = (IMAGE_ILT_ENTRY64*)((ULONG_PTR)current->FirstThunkRVA + (ULONG_PTR)imageBase);
		
		PKLINKED_LIST moduleEntry = FindInListCustomCompare(&LoadedModules, name, CompareModuleName);

		Print(L"%a:\n", name);

		if (moduleEntry == NULL)
		{
			status = TryToLoadModule(name);
			if (status)
				return EFI_LOAD_ERROR;

			moduleEntry = FindInListCustomCompare(&LoadedModules, name, CompareModuleName);

			if (moduleEntry == NULL)
				return EFI_ABORTED;
		}
		
		while (imports->AsNumber)
		{
			if (imports->Mode == 0)
			{
				Print(L"   %a\n", imports->NameRVA + (ULONG_PTR)imageBase + 2);
			}
			else
			{
				Print(L"   #%d\n", imports->Ordinal);
			}
			imports++;
		}

		current++;
	}

	return EFI_SUCCESS;
}

EFI_STATUS HandleExportDirectory(LOADED_MODULE* module, IMAGE_EXPORT_DIRECTORY_ENTRY* exportDirectoryEntry)
{
	PVOID imageBase = module->ImageBase;
	UINTN numberOfExports = exportDirectoryEntry->NumberOfFunctions;
	IMAGE_EXPORT_ADDRESS_ENTRY* exportAddressTable =
		(IMAGE_EXPORT_ADDRESS_ENTRY*) ((ULONG_PTR) exportDirectoryEntry->AddressOfFunctionsRVA + (ULONG_PTR) imageBase);

	UINTN i;

	PKLINKED_LIST exports = &module->Exports;

	module->Name = (CHAR*)((ULONG_PTR)exportDirectoryEntry->NameRVA + (ULONG_PTR)imageBase);
	module->OrdinalBase = exportDirectoryEntry->Base;

	for (i = 0; i < numberOfExports; i++)
	{
		MODULE_EXPORT* e;
		PKLINKED_LIST exportEntry = AppendList(exports, AllocateZeroPool(sizeof(MODULE_EXPORT)));

		if (exportEntry == NULL)
		{
			RemoveFromList(&LoadedModules, module);
			return EFI_OUT_OF_RESOURCES;
		}

		e = ((MODULE_EXPORT*) exportEntry->Value);
		e->Address = (PVOID) ((ULONG_PTR) exportAddressTable[i].AddressRVA + (ULONG_PTR) imageBase);
		e->Name = (PVOID) ((ULONG_PTR) exportAddressTable[i].NameRVA + (ULONG_PTR) imageBase);
	}

	return EFI_SUCCESS;
}

/* 
	LoadImage function
	Loads a PE32+ file into memory
	Recursively loads dependencies
*/
EFI_STATUS LoadImage(EFI_FILE_HANDLE file, OPTIONAL PVOID imageBase, PLOADED_MODULE* outModule)
{
	EFI_STATUS status;
	IMAGE_DOS_HEADER dosHeader;
	IMAGE_PE_HEADER peHeader;

	UINTN dataDirectoryIndex;
	DATA_DIRECTORY dataDirectories[16];
	UINTN numberOfDataDirectories, sizeOfDataDirectories;

	SECTION_HEADER* sectionHeaders;
	UINTN numberOfSectionHeaders, sizeOfSectionHeaders;
	SECTION_HEADER* currentSection;

	PKLINKED_LIST moduleLinkedListEntry;
	PLOADED_MODULE module;

	UINTN dosHeaderSize = sizeof(dosHeader);
	UINTN peHeaderSize = sizeof(peHeader);

	status = file->Read(file, &dosHeaderSize, &dosHeader);
	return_if_error(status);
	
	if (dosHeader.Signature != IMAGE_MZ_MAGIC)
		return EFI_UNSUPPORTED;

	status = file->SetPosition(file, dosHeader.e_lfanew);
	return_if_error(status);

	status = file->Read(file, &peHeaderSize, &peHeader);
	return_if_error(status);

	if (peHeader.Signature != IMAGE_PE_MAGIC)
		return EFI_UNSUPPORTED;

	if (peHeader.OptionalHeader.Signature != IMAGE_OPTIONAL_HEADER_NT64 ||
		peHeader.FileHeader.Machine != IMAGE_MACHINE_X64)
	{
		Print(L"%a: File specified is not a 64 bit executable\n", __FUNCDNAME__);
		return EFI_UNSUPPORTED;
	}

	if (imageBase == NULL)
		imageBase = (PVOID) peHeader.OptionalHeader.ImageBase;

	/* read all data directories */
	numberOfDataDirectories = peHeader.OptionalHeader.NumberOfDataDirectories;

	if (numberOfDataDirectories > 16)
		numberOfDataDirectories = 16;

	sizeOfDataDirectories = numberOfDataDirectories * sizeof(DATA_DIRECTORY);

	status = file->Read(file, &sizeOfDataDirectories, dataDirectories);
	return_if_error(status);

	/* read all sections */
	numberOfSectionHeaders = peHeader.FileHeader.NumberOfSections;
	sizeOfSectionHeaders = numberOfSectionHeaders * sizeof(SECTION_HEADER);
	sectionHeaders = AllocateZeroPool(sizeOfSectionHeaders);
	if (sectionHeaders == NULL)
		return EFI_OUT_OF_RESOURCES;
	status = file->SetPosition(file, dosHeader.e_lfanew + sizeof(peHeader) + peHeader.OptionalHeader.NumberOfDataDirectories * sizeof(DATA_DIRECTORY));
	return_if_error(status);
	status = file->Read(file, &sizeOfSectionHeaders, sectionHeaders);

	for (currentSection = sectionHeaders; currentSection < sectionHeaders + numberOfSectionHeaders; currentSection++)
	{
		UINTN sectionSize;

		status = file->SetPosition(file, currentSection->PointerToDataRVA);
		
		sectionSize = (UINTN)currentSection->SizeOfSection;
		
		if (!EFI_ERROR(status))
			status = file->Read(file, &sectionSize, (PVOID)((ULONG_PTR) currentSection->VirtualAddressRVA + (ULONG_PTR)imageBase));
		
		if (EFI_ERROR(status))
		{
			FreePool(sectionHeaders);
			return status;
		}
	}

	FreePool(sectionHeaders);

	/* add this module to the loaded module list
	if for any reason it is not possible to finish loading, remember to remove from the list */
	moduleLinkedListEntry = AppendList(&LoadedModules, AllocateZeroPool(sizeof(LOADED_MODULE)));
	if (moduleLinkedListEntry == NULL)
		return EFI_OUT_OF_RESOURCES;

	module = ((LOADED_MODULE*) moduleLinkedListEntry->Value);

	module->ImageBase = imageBase;
	module->Entrypoint = (PVOID)(peHeader.OptionalHeader.EntrypointRVA + (ULONG_PTR)imageBase);
	module->ImageSize = peHeader.OptionalHeader.SizeOfImage;
	module->Name = "";

	for (dataDirectoryIndex = 0; dataDirectoryIndex < numberOfDataDirectories; dataDirectoryIndex++)
	{
		PVOID dataDirectory = (PVOID)((ULONG_PTR) dataDirectories[dataDirectoryIndex].VirtualAddressRVA + (ULONG_PTR) imageBase);

		if (dataDirectories[dataDirectoryIndex].Size == 0 || dataDirectories[dataDirectoryIndex].VirtualAddressRVA == 0)
			continue;

		if (dataDirectoryIndex == IMAGE_DIRECTORY_ENTRY_EXPORT)
		{
			IMAGE_EXPORT_DIRECTORY_ENTRY* exportDirectoryEntry = (IMAGE_EXPORT_DIRECTORY_ENTRY*) dataDirectory;

			status = HandleExportDirectory(module, exportDirectoryEntry);
			return_if_error(status);
		}
		else if (dataDirectoryIndex == IMAGE_DIRECTORY_ENTRY_IMPORT)
		{
			IMAGE_IMPORT_DIRECTORY_ENTRY* importDirectoryEntry = (IMAGE_IMPORT_DIRECTORY_ENTRY*) dataDirectory;

			status = HandleImportDirectory(module, importDirectoryEntry);
			return_if_error(status);
		}
	}

	*outModule = module;

	return status;
}

EFI_STATUS QueryGraphicsInformation(BOOTDATA* bootdata)
{
	EFI_STATUS status;
	EFI_GRAPHICS_OUTPUT_PROTOCOL* graphicsProtocol;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* mode;

	status = gBootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, &graphicsProtocol);
	return_if_error(status);

	mode = graphicsProtocol->Mode->Info;

	bootdata->dwHeight = mode->VerticalResolution;
	bootdata->dwWidth = mode->HorizontalResolution;
	bootdata->dwPixelsPerScanline = mode->PixelsPerScanLine;
	bootdata->pdwFramebuffer = (PDWORD)graphicsProtocol->Mode->FrameBufferBase;
	bootdata->pdwFramebufferEnd = (PDWORD)((ULONG_PTR)graphicsProtocol->Mode->FrameBufferBase + (ULONG_PTR)graphicsProtocol->Mode->FrameBufferSize);

	return EFI_SUCCESS;
}

EFI_STATUS QueryMemoryMap(BOOTDATA* bootdata)
{
	EFI_STATUS status;
	UINTN memoryMapSize = 0, memoryMapKey, descriptorSize;
	EFI_MEMORY_DESCRIPTOR* memoryMap = NULL;
	UINT32 descriptorVersion;
	EFI_MEMORY_DESCRIPTOR* currentDescriptor;
	UINTN pages = 0;

	UINT8* memoryBitmap;

	do
	{
		if (memoryMap != NULL)
			FreePool(memoryMap);

		status = gBootServices->AllocatePool(EfiLoaderData, memoryMapSize, &memoryMap);
		if (EFI_ERROR(status) || memoryMap == NULL)
			return EFI_ERROR(status) ? status : EFI_OUT_OF_RESOURCES;

		status = gBootServices->GetMemoryMap(&memoryMapSize, memoryMap, &memoryMapKey, &descriptorSize, &descriptorVersion);
		
		memoryMapSize += descriptorSize;
	}
	while (status == EFI_BUFFER_TOO_SMALL);
	return_if_error(status);

	currentDescriptor = memoryMap;
	while (currentDescriptor <= (EFI_MEMORY_DESCRIPTOR*)((ULONG_PTR)memoryMap + memoryMapSize))
	{
		pages += currentDescriptor->NumberOfPages;
		currentDescriptor = (EFI_MEMORY_DESCRIPTOR*) ((ULONG_PTR)currentDescriptor + descriptorSize);
	}

	memoryBitmap = AllocateZeroPool(pages);

	currentDescriptor = memoryMap;
	while (currentDescriptor <= (EFI_MEMORY_DESCRIPTOR*) ((ULONG_PTR) memoryMap + memoryMapSize))
	{
		UINTN relativePageIndex;
		
		UINT8 memoryType = MEM_TYPE_USED;

		switch (currentDescriptor->Type)
		{
			case EfiConventionalMemory:
				memoryType = MEM_TYPE_FREE;
				break;
			case EfiLoaderCode:
			case EfiLoaderData:
			case EfiBootServicesCode:
			case EfiBootServicesData:
				memoryType = MEM_TYPE_UTIL;
				break;
		}

		for (relativePageIndex = 0; relativePageIndex < currentDescriptor->NumberOfPages; relativePageIndex++)
		{
			UINTN pageIndex = currentDescriptor->PhysicalStart / PAGE_SIZE_SMALL + relativePageIndex;

			memoryBitmap[pageIndex] = memoryType;
			
			if ((ULONG_PTR)pageIndex * PAGE_SIZE_SMALL >= (ULONG_PTR)memoryBitmap && 
				(ULONG_PTR)pageIndex * PAGE_SIZE_SMALL <= ((ULONG_PTR) memoryBitmap + pages))
			{
				memoryBitmap[pageIndex] = MEM_TYPE_USED;
				continue;
			}
		}

		currentDescriptor = (EFI_MEMORY_DESCRIPTOR*) ((ULONG_PTR) currentDescriptor + descriptorSize);
	}

	do
	{
		if (memoryMap != NULL)
			FreePool(memoryMap);

		status = gBootServices->AllocatePool(EfiLoaderData, memoryMapSize, &memoryMap);
		if (EFI_ERROR(status) || memoryMap == NULL)
			return EFI_ERROR(status) ? status : EFI_OUT_OF_RESOURCES;

		status = gBootServices->GetMemoryMap(&memoryMapSize, memoryMap, &memoryMapKey, &descriptorSize, &descriptorVersion);

		memoryMapSize += descriptorSize;
	}
	while (status == EFI_BUFFER_TOO_SMALL);
	return_if_error(status);

	bootdata->mapKey = memoryMapKey;
	bootdata->qwPhysicalMemoryMapSize = pages;
	bootdata->pPhysicalMemoryMap = memoryBitmap;

	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable)
{
	EFI_STATUS status;
	EFI_LOADED_IMAGE_PROTOCOL* loadedImage;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystem;
	EFI_FILE_HANDLE root, kernelFile;
	BOOTDATA bootdata;
	VOID (*kernelEntrypoint)(BOOTDATA*);
	PLOADED_MODULE module;

	gBootServices = systemTable->BootServices;

	InitializeLib(imageHandle, systemTable);

	status = gBootServices->HandleProtocol(imageHandle, &gEfiLoadedImageProtocolGuid, &loadedImage);
	return_if_error_debug(status);

	status = gBootServices->HandleProtocol(loadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, &filesystem);
	return_if_error_debug(status);

	status = filesystem->OpenVolume(filesystem, &root);
	return_if_error_debug(status);

	status = root->Open(root, &kernelFile, (CHAR16*)wszKernelPath, EFI_FILE_MODE_READ, 0);
	return_if_error_debug(status);

	status = LoadImage(kernelFile, (PVOID)KERNEL_INITIAL_ADDRESS, &module);
	return_if_error_debug(status);

	kernelEntrypoint = module->Entrypoint;
	Print(L"Kernel entrypoint %X\n", kernelEntrypoint);
	PrintExports();

	bootdata.KernelBase = module->ImageBase;
	bootdata.dwKernelSize = module->ImageSize;
	bootdata.ExitBootServices = gBootServices->ExitBootServices;
	bootdata.pImageHandle = imageHandle;
	
	LibGetSystemConfigurationTable(&AcpiTableGuid, &bootdata.pRdsp);

	status = QueryGraphicsInformation(&bootdata);
	return_if_error_debug(status);

	status = QueryMemoryMap(&bootdata);
	return_if_error_debug(status);

	kernelEntrypoint(&bootdata);

	ClearListAndDestroyValues(&LoadedModules, DestroyLoadedModule);

	kernelFile->Close(kernelFile);
	root->Close(root);

	Print(L"Returning to EFI\n");
	return EFI_LOAD_ERROR;
}
