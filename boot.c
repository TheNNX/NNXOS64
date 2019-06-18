
#include "gnu-efi/inc/efi.h"
#include "gnu-efi/inc/efilib.h"
#include "gnu-efi/inc/efishellintf.h"
#include "low.h"
#include "pe/pe.h"

#define MAXIMAL_STAGE2_FILESIZE 64 * 1024 //64KiB

void(*stage2_entrypoint)(EFI_STATUS *(int*, int*, UINT32, UINT32, void(*)(void*, UINTN)), void*, UINTN);

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

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	InitializeLib(ImageHandle, SystemTable);
	SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
	EFI_STATUS status;
	EFI_BOOT_SERVICES* bs = SystemTable->BootServices;
	
	UINTN mapSize = 0, mapKey, descriptorSize = 0;
	EFI_MEMORY_DESCRIPTOR *memoryMap = NULL;
	UINT32 descriptorVersion;

	status = uefi_call_wrapper((void *)gBS->GetMemoryMap, 5, &mapSize, &memoryMap, &mapKey, &descriptorSize, &descriptorVersion);
	mapSize += 16 * descriptorSize;
	status = uefi_call_wrapper((void *)gBS->AllocatePool, 3, EfiLoaderData, mapSize, (void **)&memoryMap);
	status = uefi_call_wrapper((void *)gBS->GetMemoryMap, 5, &mapSize, &memoryMap, &mapKey, &descriptorSize, &descriptorVersion);
	
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
		stage2_entrypoint(framebuffer, framebufferEnd, gop->Mode->Info->HorizontalResolution, gop->Mode->Info->VerticalResolution, (void *)gBS->ExitBootServices, ImageHandle, mapKey);
	}
	else {
		Print(L"%EError%N: NNXOSLDR.exe missing or corrupted.");
		return EFI_LOAD_ERROR;
	}

	while (1);

	return EFI_ABORTED;
}