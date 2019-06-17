
#include "gnu-efi/inc/efi.h"
#include "gnu-efi/inc/efilib.h"
#include "gnu-efi/inc/efishellintf.h"
#include "low.h"

__declspec(align(4096)) UINT64 PML4[512];

void PaginingInitialTest() {
	UINT64* CR3 = GET_CR3();

	for (int a = 0; a < 512; a++) {
		PML4[a] = CR3[a];
	}

	if (PML4[63] != CR3[63])
		Print(L"Error");
	PML4[1] = CR3[0];

	SET_CR3(PML4);
	*((UINT64*)0x8000000000) = 123456789012345678;
	*((UINT64*)0) = 7;
	*((UINT64*)0) = 740;

}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	InitializeLib(ImageHandle, SystemTable);
	SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
	EFI_STATUS status;
	EFI_BOOT_SERVICES* bs = SystemTable->BootServices;

	bs->SetWatchdogTimer(0, 0, 0, 0);
	UINTN mapSize = 0, descriptorSize;
	UINTN mapKey = 0;
	EFI_MEMORY_DESCRIPTOR *memoryMap = NULL;
	UINT32 descriptorVersion;
	Print(L"Exiting boot services...\n");
	status = uefi_call_wrapper((void *)bs->GetMemoryMap, 5, &mapSize, &memoryMap, NULL, &descriptorSize, NULL);
	mapSize += 2 * descriptorSize;
	status = uefi_call_wrapper((void *)bs->AllocatePool, 3, EfiLoaderData, mapSize, (void **)&memoryMap);
	status = uefi_call_wrapper((void *)bs->GetMemoryMap, 5, &mapSize, &memoryMap, &mapKey, &descriptorSize, &descriptorVersion);
	status = uefi_call_wrapper((void *)bs->ExitBootServices, 2, ImageHandle, mapKey);

	if (status != EFI_SUCCESS) {
		Print(L"Error of BootServices->ExitBootServices(EFI_HANDLE, UINTN): %r\n", status);
		return status;
	}

	PaginingInitialTest();
	while (1);

	return EFI_ABORTED;
}