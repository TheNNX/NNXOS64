#include "pe.h"

EFI_STATUS LoadPortableExecutable(void* fileBuffer, int bufferSize, UINT64** entrypoint) {
	IMAGE_DOS_HEADER* dos_header = fileBuffer;
	IMAGE_PE_HEADER* pe_header = (UINT64)((UINT64)dos_header + (UINT64)dos_header->e_lfanew);
	
	if (pe_header->signature != IMAGE_PE_MAGIC) 
	{
		Print(L"No magic, 0x%x\n", pe_header->signature);
		return EFI_LOAD_ERROR;
	}

	IMAGE_FILE_HEADER* fileHeader = &(pe_header->fileHeader);
	IMAGE_OPTIONAL_HEADER64* optionalHeader = &(pe_header->optionalHeader);

	UINT16 machine = fileHeader->Machine;

	if (machine != IMAGE_MACHINE_X64 || optionalHeader->signature != IMAGE_OPTIONAL_HEADER_NT64) {
		Print(L"Invalid parameters\n");
		return EFI_INVALID_PARAMETER;
	}
	UINT64 imageBase = optionalHeader->ImageBase;
	IMAGE_SECTION_TABLE_HEADER* section_table = (UINT64)(((UINT64)optionalHeader->NumberOfDataDirectories) * ((UINT64)sizeof(DataDirectory)) + ((UINT64)optionalHeader) + ((UINT64)sizeof(IMAGE_OPTIONAL_HEADER64)));
	
	for (int index = 0; index < fileHeader->NumberOfSections; index++)
	{
		SECTION_HEADER* SectionHeader = section_table->headers+index;

		CHAR16 name[8];
		for (int a = 0; a < 8; a++) {
			name[a] = SectionHeader->Name[a];
		}

		Print(L"  Section '%s': VA:0x%x SoS:0x%x\n", name, SectionHeader->VirtualAddress, SectionHeader->SizeOfSection);

		
		UINT8* dst = SectionHeader->VirtualAddress + imageBase;
		UINT8* src = SectionHeader->SectionPointer + (UINT64)fileBuffer;

		for (int memory = 0; memory < SectionHeader->SizeOfSection; memory++) {
			dst[memory] = src[memory];
		}

	}

	int numberOfDataEntries = optionalHeader->NumberOfDataDirectories;

	IMAGE_EXPORT_TABLE* exportTable = optionalHeader->dataDirectories[IMAGE_DIRECTORY_ENTRY_EXPORT].virtualAddress + imageBase;
	IMAGE_IMPORT_TABLE* importTable = optionalHeader->dataDirectories[IMAGE_DIRECTORY_ENTRY_IMPORT].virtualAddress + imageBase;
	RVA importTableRVA = optionalHeader->dataDirectories[IMAGE_DIRECTORY_ENTRY_IMPORT].virtualAddress;
	RVA exportTableRVA = optionalHeader->dataDirectories[IMAGE_DIRECTORY_ENTRY_EXPORT].virtualAddress;

	Print(L"%d\n",numberOfDataEntries);

	if (numberOfDataEntries > IMAGE_DIRECTORY_ENTRY_IMPORT && importTableRVA) {
		Print(L"Import directory exists\n");
		int entryIndex = 0; 

		while (importTable->entries[entryIndex].OriginalFirstThunk)
		{
			Print(L" | Import: %a\n", imageBase+importTable->entries[entryIndex].Name);
			while (*((RVA**)(imageBase + importTable->entries[entryIndex].FirstThunk))) {

				IMAGE_IMPORT_BY_NAME* importByName = (imageBase + *((RVA**)(imageBase + importTable->entries[entryIndex].FirstThunk)));

				Print(L" |--> %a\n",imageBase+importByName->Name);

				importTable->entries[entryIndex].FirstThunk += sizeof(RVA*);
			}
			entryIndex++;
		}

		return EFI_UNSUPPORTED;
	}

	*entrypoint = (UINT64*)(imageBase + optionalHeader->EntrypointRVA);

	return EFI_SUCCESS;
}

