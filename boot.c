
#include "gnu-efi/inc/efi.h"
#include "gnu-efi/inc/efilib.h"
#include "gnu-efi/inc/efishellintf.h"
#include "low.h"
#include "pe/pe.h"

void(*Stage2entrypoint)(EFI_STATUS *(int*, int*, UINT32, UINT32, void(*)(void*, UINTN)), void*, UINTN, UINT8*, UINT64, UINT64, void*);


EFI_FILE_PROTOCOL* GetFile(EFI_FILE* Root, CHAR16* Name) {
	EFI_STATUS  Status = 0;
	EFI_FILE_PROTOCOL *File = 0;

	Status = Root->Open(Root, &File, Name, EFI_FILE_MODE_READ, 0);

	if (EFI_ERROR(Status)) {
		return 0;
	}

	return File;
}

UINTN MapSize = 0, MapKey, DescriptorSize = 0;
EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
UINT32 DescriptorVersion;

void ScanAndPrintMemoryMap(UINT64* FreePages, UINT64* TotPages, UINT64* OutMapKey) {

	EFI_STATUS Status;
	while (EFI_SUCCESS != (Status = uefi_call_wrapper((void *)gBS->GetMemoryMap, 5, &MapSize,
		MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion)))
	{
		if (Status == EFI_BUFFER_TOO_SMALL)
		{
			MapSize += 2 * DescriptorSize;
			uefi_call_wrapper((void *)gBS->AllocatePool, 3, EfiLoaderData, MapSize, (void **)&MemoryMap);
		}
	}


	UINT64 StartOfMemoryMap = MemoryMap;
	UINT64 EndOfMemoryMap = StartOfMemoryMap + MapSize;

	UINT64 Offset = StartOfMemoryMap;

	UINT64 Index = 0;
	int PrintN = 61;

	while (Offset < EndOfMemoryMap)
	{
		EFI_MEMORY_DESCRIPTOR *Desc = (EFI_MEMORY_DESCRIPTOR *)Offset;

		for (int a = 0; a < 1 + (Desc->NumberOfPages / 64); a++) {
			if (PrintN > 60)
			{
				PrintN = 0;
				Print(L"\n%ll016X: ", Desc->PhysicalStart + a * 64 * 4096);
			}
			if (Desc->Type == EfiConventionalMemory)
				Print(L"_");
			else if (Desc->Type == EfiMemoryMappedIO || Desc->Type == EfiMemoryMappedIOPortSpace)
				Print(L"I");
			else if (Desc->Type == EfiReservedMemoryType)
				Print(L"█");
			else if (Desc->Type == EfiUnusableMemory)
				Print(L"U");
			else if (Desc->Type == EfiLoaderCode || Desc->Type == EfiLoaderData)
				Print(L"░");
			else if (Desc->Type == EfiBootServicesCode || Desc->Type == EfiBootServicesData)
				Print(L"░");
			else if (Desc->Type == EfiRuntimeServicesCode || Desc->Type == EfiRuntimeServicesData)
				Print(L"R");
			else if (Desc->Type == EfiACPIMemoryNVS || Desc->Type == EfiACPIReclaimMemory)
				Print(L"A");
			else if (Desc->Type == EfiPalCode)
				Print(L"P");
			else
				Print(L"?");
			PrintN++;
			for (UINT64 timer = 0; timer < 0xFFF; timer++);
		}

		Offset += DescriptorSize;
		*TotPages += Desc->NumberOfPages;

		if (Desc->Type == EfiConventionalMemory || Desc->Type == EfiLoaderCode || Desc->Type == EfiLoaderData || Desc->Type == EfiBootServicesCode || Desc->Type == EfiBootServicesData)
			*FreePages += Desc->NumberOfPages;

		Index++;
	}
	Print(L"\n");
	*OutMapKey = MapKey;
}

void BackspaceChar() {
	EFI_SIMPLE_TEXT_OUT_PROTOCOL* Output = gST->ConOut;
	EFI_SIMPLE_TEXT_IN_PROTOCOL* Input = gST->ConIn;
	UINTN X, Y;
	UINTN Columns, Rows;

	Output->QueryMode(Output, Output->Mode, &Columns, &Rows);
	
	X = Output->Mode->CursorColumn;
	Y = Output->Mode->CursorRow;

	if(X)
		X--;
	else {
		X = Columns - 1;

		if(Y)
			Y--;
	}
	PrintAt(X, Y, L" ");
	Output->SetCursorPosition(Output, X, Y);
}

UINTN AskUserForNumber() {
	UINTN CurrentNumber = 0, BufferLen = 0;
	EFI_STATUS Status;
	EFI_SIMPLE_TEXT_IN_PROTOCOL* Input = gST->ConIn;
	EFI_INPUT_KEY Keydata;

	while (TRUE) {
		UINTN Index;
		gBS->WaitForEvent(1, &Input->WaitForKey, &Index);
		Status = Input->ReadKeyStroke(Input, &Keydata);
		if (!EFI_ERROR(Status)) {
			if (Keydata.UnicodeChar == 0x08) {
				if (BufferLen) {
					CurrentNumber /= 10;
					BufferLen--;
					BackspaceChar();
				}
			}
			else if(Keydata.UnicodeChar >= L'0' && Keydata.UnicodeChar <= L'9' && CurrentNumber < 100){
				UINTN Inserted = Keydata.UnicodeChar - L'0';
				CurrentNumber *= 10;
				CurrentNumber += Inserted;
				BufferLen++;
				Print(L"%lc", Keydata.UnicodeChar);
			}
			else if (Keydata.UnicodeChar == 0xd || Keydata.UnicodeChar == 0xa) {
				Print(L"\n");
				return CurrentNumber;
			}
		}
		else {
			Print(L"Error reading key.\n");
			return CurrentNumber;
		}
	}

}

typedef struct BootmenuItem {
	CHAR16 Name[16];
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Filesystem;
	struct BootmenuItem* Next;
} BootmenuItem;

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	InitializeLib(ImageHandle, SystemTable);
	SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
	void* pAcpiPointer;
	LibGetSystemConfigurationTable(&AcpiTableGuid, (VOID*)&pAcpiPointer);
	
	EFI_STATUS Status;
	
	gBS = SystemTable->BootServices;

	UINT64 Pages = 0, FreePages = 0;
	
	ScanAndPrintMemoryMap(&FreePages, &Pages, &MapKey);

	UINT8* NNXMMap = 0;
	Status = gBS->AllocatePool(EfiBootServicesData, Pages, &NNXMMap);
	if (Status != EFI_SUCCESS) {
		Print(L"Error: could not allocate memory for memory map...\n");
		return Status;
	}

	UINT64 Offset = MemoryMap;
	UINT64 NNXMMapIndex = 0;
	while (Offset < ((UINT64)MemoryMap) + MapSize)
	{
		EFI_MEMORY_DESCRIPTOR *Desc = (EFI_MEMORY_DESCRIPTOR *)Offset;
		for (UINT64 a = 0; a < Desc->NumberOfPages; a++) {
			if (NNXMMapIndex*4096 >= NNXMMap && NNXMMapIndex*4096 <= (((UINT64)NNXMMap) + Pages)) {
				NNXMMap[NNXMMapIndex] = 0;
				NNXMMapIndex++;
				continue;
			}
			
			if (Desc->Type == EfiConventionalMemory)
				NNXMMap[NNXMMapIndex] = 1;
			else if (Desc->Type == EfiLoaderCode || Desc->Type == EfiLoaderData || Desc->Type == EfiBootServicesCode || Desc->Type == EfiBootServicesData)
				NNXMMap[NNXMMapIndex] = 2;
			else
				NNXMMap[NNXMMapIndex] = 0;
			NNXMMapIndex++;
		}
		Offset += DescriptorSize;
	}
	Print(L"Memory map has been filled up to index %d\n",NNXMMapIndex);
	Print(L"%u B [%u MiB] RAM free out of %u B [%u MiB] total\n",FreePages*4096,FreePages/256,Pages*4096,Pages/256);
	Print(L"%lf%% memory free to use.\n",((double)FreePages)/((double)Pages)*100.0);

	UINT32* Framebuffer;
	UINT32* FramebufferEnd;

	EFI_HANDLE* HandleBuffer;
	UINTN HandleCount = 0;
	EFI_GRAPHICS_OUTPUT_PROTOCOL* GOP;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* GOPModeInfo;
	Status = gBS->LocateHandleBuffer(ByProtocol,
		&gEfiGraphicsOutputProtocolGuid,
		NULL,
		&HandleCount,
		&HandleBuffer);

	Status = gBS->HandleProtocol(HandleBuffer[0],
		&gEfiGraphicsOutputProtocolGuid,
		(VOID **)&GOP);

	Framebuffer = GOP->Mode->FrameBufferBase;
	FramebufferEnd = GOP->Mode->FrameBufferSize + GOP->Mode->FrameBufferBase;

	for (UINT64 Times = 0; Times < GOP->Mode->FrameBufferSize / 4096; Times++) {
		NNXMMap[Times + ((UINT64)Framebuffer)/4096] = 0;
	}

	EFI_HANDLE* Filesystems;
	UINTN FilesystemCount;

	Status = gBS->LocateHandleBuffer(ByProtocol,
		&gEfiSimpleFileSystemProtocolGuid,
		NULL,
		&FilesystemCount,
		&Filesystems);

	if (EFI_ERROR(Status))
	{
		Print("Error: Devices not found\n");
		return EFI_NOT_FOUND;
	}

	gST->ConOut->ClearScreen(gST->ConOut);

	UINTN MenuItemCount = 0;
	BootmenuItem* Bootmenu = 0;

	for (int i = 0; i < FilesystemCount; i++) {
		EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SF;
		Status = gBS->HandleProtocol(Filesystems[i], &gEfiSimpleFileSystemProtocolGuid, (VOID**)&SF);
		Print(L"Enumerating handle no. %d\n", i);
		
		if (EFI_ERROR(Status))
		{
			Print(L"Error: Device %d not found\n", i);
			continue;
		}

		EFI_FILE *RootFile;
		Status = SF->OpenVolume(SF, &RootFile);

		if (EFI_ERROR(Status))
		{
			Print(L"Error: root directory not found\n");
			continue;
		}

		EFI_FILE* NNXOSCFGTXT = GetFile(RootFile, L"efi\\boot\\NNXOSCFG.txt");
		EFI_FILE* NNXOSLDREXE = GetFile(RootFile, L"efi\\boot\\NNXOSLDR.exe");

		if (NNXOSLDREXE == 0)
		{
			Print(L"NNXOSLDR not present on the volume, going to the next one.\n");
			RootFile->Close(RootFile);
			continue;
		}

		MenuItemCount++;
		CHAR16 Info16[16] = L"NO NAME";

		if (NNXOSCFGTXT == 0) {
			Print(L"No config file, defaulting to NO NAME");
		}
		else {

			CHAR8 Info[16] = { 0 };
			UINTN MaximalSize = 15 * sizeof(*Info);
			Status = NNXOSCFGTXT->Read(NNXOSCFGTXT, &MaximalSize, Info);
			if (EFI_ERROR(Status)) {
				Print(L"Error reading config file\n");
			}

			for (int a = 0; a < 16; a++) {
				Info16[a] = (CHAR16)Info[a];
			}

			RootFile->Close(NNXOSCFGTXT);
		}
		RootFile->Close(NNXOSLDREXE);
		RootFile->Close(RootFile);

		BootmenuItem** Current = &Bootmenu;
		while (*Current)
			Current = &((*Current)->Next);

		*Current = AllocateZeroPool(sizeof(BootmenuItem));
		CopyMem((*Current)->Name, Info16, 16*sizeof(CHAR16));
		(*Current)->Filesystem = SF;
	}

	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* SelectedSF;

	if (MenuItemCount == 1) {
		Print(L"One menu item avaible.\n");
		SelectedSF = Bootmenu->Filesystem;
	}
	else if (MenuItemCount > 1) {
		EFI_SIMPLE_FILE_SYSTEM_PROTOCOL** ChoiceArray = AllocateZeroPool(sizeof(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL) * MenuItemCount);
		BootmenuItem* CurrentItem = Bootmenu;
		UINTN Number = 1;
		while (Bootmenu)
		{
			ChoiceArray[Number - 1] = Bootmenu->Filesystem;
			Print(L"%d. %s\n", Number, Bootmenu->Name);
			Bootmenu = Bootmenu->Next;
			Number++;
		}
		Print(L"Multiple boot partitions avaible:\n");
		while (Number > MenuItemCount || Number < 1) {
			Print(L"Please give a number between %d and %d.\n", 1, MenuItemCount);
			Number = AskUserForNumber();
		}
		Print(L"%x selected\n", ChoiceArray[Number - 1]);
		SelectedSF = ChoiceArray[Number - 1];
		
	}else{
		return EFI_NOT_FOUND;
	}

	EFI_FILE* Root;
	Status = SelectedSF->OpenVolume(SelectedSF, &Root);

	if (EFI_ERROR(Status)) {
		Print(L"Error: cannot open the root directory.\n");
		return EFI_NO_MEDIA;
	}

	EFI_FILE* FP = GetFile(Root, L"efi\\boot\\NNXOSLDR.exe");

	if (FP == 0) {
		Print(L"Error: Unable to open the executable.\n");
		return EFI_NOT_FOUND;
	}

	
	char FInfoBuffer[512];
	UINTN FInfoBufferSize = sizeof(FInfoBuffer);
	EFI_FILE_INFO *FInfo = (void*)FInfoBuffer;
	FP->GetInfo(FP, &gEfiFileInfoGuid, &FInfoBufferSize, FInfo);
	CHAR8 *FileBuffer = AllocateZeroPool(FInfo->FileSize);
	FP->Read(FP, &FInfo->FileSize, FileBuffer);
	FP->Close(FP);

	if (FileBuffer[0] == 'M' && FileBuffer[1] == 'Z') {
		Status = LoadPortableExecutable(FileBuffer, FInfo->FileSize, &Stage2entrypoint, NNXMMap);

		if (EFI_ERROR(Status)) {
			Print(L"%r\n",Status);
			return Status;
		}
		
		gBS->SetWatchdogTimer(0, 0, 0, NULL);
		Stage2entrypoint(Framebuffer, FramebufferEnd, GOP->Mode->Info->HorizontalResolution, GOP->Mode->Info->VerticalResolution, (void *)gBS->ExitBootServices, ImageHandle, MapKey, 
			NNXMMap, NNXMMapIndex, Pages * 4096, pAcpiPointer);
	}
	else {
		Print(L"%EError%N: NNXOSLDR.exe missing or corrupted. Are you sure you're trying to boot of the correct partition?");
		return EFI_LOAD_ERROR;
	}

	while (1);
	
	return EFI_ABORTED;
}