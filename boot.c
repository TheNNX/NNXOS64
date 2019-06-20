
#include "gnu-efi/inc/efi.h"
#include "gnu-efi/inc/efilib.h"
#include "gnu-efi/inc/efishellintf.h"
#include "low.h"
#include "pe/pe.h"

#define MAXIMAL_STAGE2_FILESIZE 64 * 1024 //64KiB

EFI_BOOT_SERVICES* bs;

void(*stage2_entrypoint)(EFI_STATUS *(int*, int*, UINT32, UINT32, void(*)(void*, UINTN)), void*, UINTN, UINT8*, UINT64);


EFI_FILE_PROTOCOL* GetFile(CHAR16* name) {
	EFI_STATUS  Status = 0;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystem;
	EFI_FILE_PROTOCOL *Root = 0;
	EFI_FILE_PROTOCOL *file = 0;

	Status = gBS->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid, NULL, (VOID**)&SimpleFileSystem);

	if (EFI_ERROR(Status)) {
		Print(L"Error at: locating protocol\n");
		return 0;
	}
	Status = SimpleFileSystem->OpenVolume(SimpleFileSystem, &Root);

	if (EFI_ERROR(Status)) {
		Print(L"Error at: opening volume\n");
		return 0;
	}
	Status = Root->Open(Root, &file, name, EFI_FILE_MODE_READ, 0);

	if (EFI_ERROR(Status)) {
		Print(L"Error at: opening file '%s'\n", name);
		return 0;
	}

	return file;
}

EFI_STATUS status = 0;
UINTN mapSize = 0, mapKey, descriptorSize = 0;
EFI_MEMORY_DESCRIPTOR *memoryMap = NULL;
UINT32 descriptorVersion;

void scanAndPrintMemoryMap(UINT64* freePages, UINT64* totPages, UINT64* outMapKey) {

	while (EFI_SUCCESS != (status = uefi_call_wrapper((void *)bs->GetMemoryMap, 5, &mapSize,
		memoryMap, &mapKey, &descriptorSize, &descriptorVersion)))
	{
		if (status == EFI_BUFFER_TOO_SMALL)
		{
			mapSize += 2 * descriptorSize;
			uefi_call_wrapper((void *)bs->AllocatePool, 3, EfiLoaderData, mapSize, (void **)&memoryMap);
		}
	}


	UINT64 startOfMemoryMap = memoryMap;
	UINT64 endOfMemoryMap = startOfMemoryMap + mapSize;

	UINT64 offset = startOfMemoryMap;

	UINT64 index = 0;
	int printN = 61;

	while (offset < endOfMemoryMap)
	{
		EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)offset;

		for (int a = 0; a < 1 + (desc->NumberOfPages / 64); a++) {
			if (printN > 60)
			{
				printN = 0;
				Print(L"\n%ll016X: ", desc->PhysicalStart + a * 64 * 4096);
			}
			if (desc->Type == EfiConventionalMemory)
				Print(L"_");
			else if (desc->Type == EfiMemoryMappedIO || desc->Type == EfiMemoryMappedIOPortSpace)
				Print(L"I");
			else if (desc->Type == EfiReservedMemoryType)
				Print(L"█");
			else if (desc->Type == EfiUnusableMemory)
				Print(L"U");
			else if (desc->Type == EfiLoaderCode || desc->Type == EfiLoaderData)
				Print(L"░");
			else if (desc->Type == EfiBootServicesCode || desc->Type == EfiBootServicesData)
				Print(L"░");
			else if (desc->Type == EfiRuntimeServicesCode || desc->Type == EfiRuntimeServicesData)
				Print(L"R");
			else if (desc->Type == EfiACPIMemoryNVS || desc->Type == EfiACPIReclaimMemory)
				Print(L"A");
			else if (desc->Type == EfiPalCode)
				Print(L"P");
			else
				Print(L"?");
			printN++;
			for (UINT64 timer = 0; timer < 0xFFF; timer++);
		}

		offset += descriptorSize;
		*totPages += desc->NumberOfPages;

		if (desc->Type == EfiConventionalMemory || desc->Type == EfiLoaderCode || desc->Type == EfiLoaderData || desc->Type == EfiBootServicesCode || desc->Type == EfiBootServicesData)
			*freePages += desc->NumberOfPages;

		index++;
	}
	Print(L"\n");
	*outMapKey = mapKey;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	InitializeLib(ImageHandle, SystemTable);
	SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
	EFI_STATUS status;
	bs = SystemTable->BootServices;

	UINT64 pages = 0;
	UINT64 freePages = 0;
	
	scanAndPrintMemoryMap(&freePages, &pages, &mapKey);

	UINT8* nnxMMap = 0;
	status = bs->AllocatePool(EfiBootServicesData, pages, &nnxMMap);
	if (status != EFI_SUCCESS) {
		Print(L"Error: could not allocate memory for memory map...\n");
		return status;
	}

	UINT64 offset = memoryMap;
	UINT64 nnxMMapIndex = 0;
	while (offset < ((UINT64)memoryMap) + mapSize)
	{
		EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)offset;
		for (UINT64 a = 0; a < desc->NumberOfPages; a++) {
			if (desc->Type == EfiConventionalMemory || desc->Type == EfiLoaderCode || desc->Type == EfiLoaderData || desc->Type == EfiBootServicesCode || desc->Type == EfiBootServicesData)
				nnxMMap[nnxMMapIndex] = 1;
			else
				nnxMMap[nnxMMapIndex] = 0;
			nnxMMapIndex++;
		}
		offset += descriptorSize;
	}
	Print(L"Memory map has been filled up to index %d\n",nnxMMapIndex);
	Print(L"%u B [%u MiB] RAM free out of %u B [%u MiB] total\n",freePages*4096,freePages/256,pages*4096,pages/256);
	Print(L"%lf%% memory free to use.\n",((double)freePages)/((double)pages)*100.0);

	UINT32* framebuffer;
	UINT32* framebufferEnd;

	EFI_HANDLE* handle_buffer;
	UINTN handle_count = 0;
	EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* gop_mode_info;
	status = gBS->LocateHandleBuffer(ByProtocol,
		&gEfiGraphicsOutputProtocolGuid,
		NULL,
		&handle_count,
		&handle_buffer);

	status = gBS->HandleProtocol(handle_buffer[0],
		&gEfiGraphicsOutputProtocolGuid,
		(VOID **)&gop);

	framebuffer = gop->Mode->FrameBufferBase;
	framebufferEnd = gop->Mode->FrameBufferSize + gop->Mode->FrameBufferBase;

	for (UINT32* a = framebuffer; a < framebufferEnd; a++) {
		*a = 0x7f7f7f7f;
	}
	Print(L"%dx%d - %d\n", gop->Mode->Info->HorizontalResolution, gop->Mode->Info->VerticalResolution, gop->Mode->Info->PixelFormat);

	EFI_FILE_PROTOCOL* fp = GetFile(L"NNXOSLDR.exe");

	int limitBufferSize = MAXIMAL_STAGE2_FILESIZE;
	CHAR8 fileBuffer[MAXIMAL_STAGE2_FILESIZE];
	char finfoBuffer[512];
	UINTN finfoBufferSize = sizeof(finfoBuffer);
	EFI_FILE_INFO *finfo = (void*)finfoBuffer;
	fp->GetInfo(fp, &gEfiFileInfoGuid, &finfoBufferSize, finfo);
	if (limitBufferSize > finfo->FileSize)
		limitBufferSize = finfo->FileSize;
	fp->Read(fp, &limitBufferSize, fileBuffer);
	fp->Close(fp);

	if (fileBuffer[0] == 'M' && fileBuffer[1] == 'Z') {
		int status = LoadPortableExecutable(fileBuffer, limitBufferSize, &stage2_entrypoint);

		if (status) {
			Print(L"%r\n",status);
			return status;
		}

		gBS->SetWatchdogTimer(0, 0, 0, NULL);
		stage2_entrypoint(framebuffer, framebufferEnd, gop->Mode->Info->HorizontalResolution, gop->Mode->Info->VerticalResolution, (void *)gBS->ExitBootServices, ImageHandle, mapKey, nnxMMap, nnxMMapIndex);
	}
	else {
		Print(L"%EError%N: NNXOSLDR.exe missing or corrupted.");
		return EFI_LOAD_ERROR;
	}

	while (1);

	return EFI_ABORTED;
}