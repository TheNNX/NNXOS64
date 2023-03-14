/* TODO: separate out PE32 loading */
/* TODO: finish import resolving */

#pragma warning(disable: 4142)
#define EFI_NT_EMUL
#include <ntlist.h>
#include <efi.h>
#include <efilib.h>
#include <nnxtype.h>
#include <nnxcfg.h>
#include <nnxpe.h>
#include <bootdata.h>

#define ALLOC(x) AllocateZeroPool(x)
#define DEALLOC(x) FreePool(x)
#include <HAL/physical_allocator.h>

#define RETURN_IF_ERROR_A(status, d) \
    if (EFI_ERROR(status)) \
    { \
        if(d) \
            Print(L"Line: %d Status: %r\n", __LINE__, status); \
        return status; \
    }
#define RETURN_IF_ERROR(status) RETURN_IF_ERROR_A(status, 0)
#define RETURN_IF_ERROR_DEBUG(status) RETURN_IF_ERROR_A(status, 1)

EFI_STATUS
LoadImage(
    EFI_FILE_HANDLE file,
    PLOADED_BOOT_MODULE * outModule);

const CHAR16 *wszKernelPath = L"efi\\boot\\NNXOSKRN.exe";
EFI_BOOT_SERVICES* gBootServices;
LIST_ENTRY LoadedModules;

VOID 
DestroyLoadedModule(PVOID modulePointer)
{
    LOADED_BOOT_MODULE* module = (LOADED_BOOT_MODULE*) modulePointer;
    FreePool(module);
}

EFI_STATUS 
TryToLoadModule(
    const CHAR* name,
    EFI_FILE_HANDLE fs,
    PLOADED_BOOT_MODULE* pOutModule)
{
    EFI_STATUS status;
    EFI_FILE_HANDLE moduleFile;
    SIZE_T Idx;
    WCHAR TempBuffer[256];

    Idx = 0;

    while (name[Idx] != 0 && Idx < 255)
    {
        TempBuffer[Idx] = (WCHAR)name[Idx];
        Idx++;
    }
    TempBuffer[Idx++] = 0;

    status = fs->Open(
        fs,
        &moduleFile,
        TempBuffer,
        EFI_FILE_MODE_READ,
        0);
    RETURN_IF_ERROR_DEBUG(status);

    return LoadImage(moduleFile, pOutModule);
}

LOADED_BOOT_MODULE*
TryFindLoadedModule(
    const CHAR* Name)
{
    LOADED_BOOT_MODULE* module = NULL;
    PLIST_ENTRY curModuleEntr;

    curModuleEntr = LoadedModules.Flink;
    while (curModuleEntr != &LoadedModules)
    {
        module = CONTAINING_RECORD(
            curModuleEntr,
            LOADED_BOOT_MODULE,
            ListEntry);

        if (strcmpa(module->Name, Name) == 0)
        {
            return module;
        }

        curModuleEntr = curModuleEntr->Flink;
    }

    return NULL;
}

EFI_STATUS
TryFindLoadedExport(
    PLOADED_BOOT_MODULE Module, 
    CHAR8* ImportName, 
    USHORT ImportOrdinal,
    PBOOT_MODULE_EXPORT* pOutExport)
{
    SIZE_T i;

    if (ImportName == NULL &&
        ImportOrdinal >= Module->NumberOfExports)
    {
        Print(L"Import by ordinal failed for #%d.\n", ImportOrdinal);
        return EFI_NOT_FOUND;
    }

    if (ImportName == NULL)
    {
        Print(L"Import by ordinal done #%d.\n", ImportOrdinal);
        *pOutExport = &Module->Exports[ImportOrdinal];
        return EFI_SUCCESS;
    }

    for (i = 0; i < Module->NumberOfExports; i++)
    {
        if (ImportName != NULL &&
            Module->Exports[i].ExportName != NULL &&
            Module->Exports[i].ExportNameRva != NULL &&
            strcmpa(Module->Exports[i].ExportName, ImportName) == 0)
        {
            *pOutExport = &Module->Exports[i];
            return EFI_SUCCESS;
        }
    }

    Print(L"Import for %a failed.\n", ImportName);
    return EFI_NOT_FOUND;
}

EFI_STATUS 
HandleImportDirectory(
    LOADED_BOOT_MODULE* Dependent,
    EFI_FILE_HANDLE SearchDirectory,
    IMAGE_IMPORT_DIRECTORY_ENTRY* ImportDirectoryEntry)
{
    EFI_STATUS Status;
    ULONG_PTR ImageBase = Dependent->ImageBase;
    INT UnresolvedImports = 0;

    IMAGE_IMPORT_DESCRIPTOR* Current = ImportDirectoryEntry->Entries;

    while (Current->NameRVA != 0)
    {
        CHAR* ModuleName = 
            (CHAR*)((ULONG_PTR)Current->NameRVA + (ULONG_PTR)ImageBase);
        Print(L"%a:\n", ModuleName);
        
        IMAGE_ILT_ENTRY64* CurrentImport = 
            (IMAGE_ILT_ENTRY64*)((ULONG_PTR)Current->OriginalFirstThunk + 
                                 (ULONG_PTR)ImageBase);
        
        LOADED_BOOT_MODULE* Dependency = TryFindLoadedModule(ModuleName);

        if (Dependency == NULL)
        {
            Status = TryToLoadModule(ModuleName, SearchDirectory, &Dependency);
            if (Status)
                return EFI_LOAD_ERROR;
        }
        
        Print(L"Imports:\n");
        while (CurrentImport->AsNumber)
        {
            PBOOT_MODULE_EXPORT Export = NULL;

            if (CurrentImport->Mode == 0)
            {
                /* +2 to skip the Hint */
                CHAR8* ImportName = (CHAR8*)
                    (CurrentImport->NameRVA + (ULONG_PTR)ImageBase + 2);

                Status = TryFindLoadedExport(
                    Dependency, 
                    ImportName, 
                    0, 
                    &Export);

                if (Status != EFI_SUCCESS)
                {
                    UnresolvedImports++;
                    Print(
                        L"    Unresolved name import %a from %a\n",
                        ImportName, 
                        ModuleName);
                }
            }
            else
            {
                Status = TryFindLoadedExport(
                    Dependency,
                    NULL,
                    CurrentImport->Ordinal,
                    &Export);
                if (Status != EFI_SUCCESS)
                {
                    Print(
                        L"    Unresolved ordinal import #%d from \n",
                        CurrentImport->Ordinal,
                        ModuleName);

                    UnresolvedImports++;
                }
            }
            if (Export != NULL)
            {
                *((PULONG_PTR)CurrentImport) = Export->ExportAddress;
            }
            CurrentImport++;
        }

        Current++;
    }

    if (UnresolvedImports > 0)
    {
        Print(L"Unresolved imports: %d.\n", UnresolvedImports);
        return EFI_NOT_FOUND;
    }
    Print(L"No unresolved imports.\n");
    return EFI_SUCCESS;
}

EFI_STATUS 
LoadImage(
    EFI_FILE_HANDLE file,
    PLOADED_BOOT_MODULE* outModule)
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
    ULONG_PTR imageBase;

    PLOADED_BOOT_MODULE Module;

    UINTN dosHeaderSize = sizeof(dosHeader);
    UINTN peHeaderSize = sizeof(peHeader);

    status = file->Read(file, &dosHeaderSize, &dosHeader);
    RETURN_IF_ERROR(status);
    
    if (dosHeader.Signature != IMAGE_MZ_MAGIC)
        return EFI_UNSUPPORTED;

    status = file->SetPosition(file, dosHeader.e_lfanew);
    RETURN_IF_ERROR(status);

    status = file->Read(file, &peHeaderSize, &peHeader);
    RETURN_IF_ERROR(status);

    if (peHeader.Signature != IMAGE_PE_MAGIC)
        return EFI_UNSUPPORTED;

    if (peHeader.OptionalHeader.Signature != IMAGE_OPTIONAL_HEADER_NT64 ||
        peHeader.FileHeader.Machine != IMAGE_MACHINE_X64)
    {
        Print(L"%a: File specified is not a 64 bit executable\n", __FUNCDNAME__);
        return EFI_UNSUPPORTED;
    }

    status = gBootServices->AllocatePages(
        AllocateAnyPages, 
        EfiLoaderCode,
        512, 
        &imageBase);
    Print(L"Image base %X\n", imageBase);
    RETURN_IF_ERROR_DEBUG(status);

    /* Read all data directories. */
    numberOfDataDirectories = peHeader.OptionalHeader.NumberOfDataDirectories;

    if (numberOfDataDirectories > 16)
        numberOfDataDirectories = 16;

    sizeOfDataDirectories = numberOfDataDirectories * sizeof(DATA_DIRECTORY);

    status = file->Read(file, &sizeOfDataDirectories, dataDirectories);
    RETURN_IF_ERROR(status);

    /* Read all sections. */
    numberOfSectionHeaders = peHeader.FileHeader.NumberOfSections;
    sizeOfSectionHeaders = numberOfSectionHeaders * sizeof(SECTION_HEADER);
    sectionHeaders = AllocateZeroPool(sizeOfSectionHeaders);
    if (sectionHeaders == NULL)
        return EFI_OUT_OF_RESOURCES;

    status = file->SetPosition(
        file, 
        dosHeader.e_lfanew + 
            sizeof(peHeader) + 
            peHeader.OptionalHeader.NumberOfDataDirectories * 
                sizeof(DATA_DIRECTORY));

    RETURN_IF_ERROR(status);
    status = file->Read(
        file, 
        &sizeOfSectionHeaders, 
        sectionHeaders);

    for (currentSection = sectionHeaders; 
         currentSection < sectionHeaders + numberOfSectionHeaders;
         currentSection++)
    {
        UINTN sectionSize;

        status = file->SetPosition(file, currentSection->PointerToDataRVA);
        sectionSize = (UINTN)currentSection->SizeOfSection;
        
        if (!EFI_ERROR(status))
        {
            status = file->Read(
                file,
                &sectionSize,
                (PVOID)((ULONG_PTR)currentSection->VirtualAddressRVA + imageBase));
        }

        if (EFI_ERROR(status))
        {
            FreePool(sectionHeaders);
            return status;
        }
    }

    /* Add this module to the loaded module list. */
    Module = AllocateZeroPool(sizeof(LOADED_BOOT_MODULE));
    if (Module == NULL)
        return EFI_OUT_OF_RESOURCES;
    InsertTailList(&LoadedModules, &Module->ListEntry);

    Module->ImageBase = imageBase;
    Module->Entrypoint = 
        (PVOID)(peHeader.OptionalHeader.EntrypointRVA + imageBase);
    Module->ImageSize = peHeader.OptionalHeader.SizeOfImage;
    Module->Name = "";
    Module->SectionHeaders = sectionHeaders;
    Module->NumberOfSectionHeaders = numberOfSectionHeaders;
    Module->Exports = NULL;
    Module->NumberOfExports = 0;

    for (dataDirectoryIndex = 0;
         dataDirectoryIndex < numberOfDataDirectories;
         dataDirectoryIndex++)
    {
        PVOID dataDirectory = 
            (PVOID)(
                (ULONG_PTR) dataDirectories[dataDirectoryIndex].VirtualAddressRVA + 
                imageBase);

        if (dataDirectories[dataDirectoryIndex].Size == 0 || 
            dataDirectories[dataDirectoryIndex].VirtualAddressRVA == 0)
            continue;

        if (dataDirectoryIndex == IMAGE_DIRECTORY_ENTRY_EXPORT)
        {
            IMAGE_EXPORT_DIRECTORY_ENTRY* exportDirectoryEntry = 
                (IMAGE_EXPORT_DIRECTORY_ENTRY*) dataDirectory;

            RVA* NamePointerTable = (RVA*)
                (exportDirectoryEntry->AddressOfNamesRVA + imageBase);
            USHORT* Ordinals = (USHORT*)
                (exportDirectoryEntry->AddressOfNameOrdinalsRVA + imageBase);
            /* TODO: handle forwarder RVAs */
            RVA* Addresses = (RVA*)
                (exportDirectoryEntry->AddressOfFunctionsRVA + imageBase);
            USHORT OrdinalBase = 
                exportDirectoryEntry->OrdinalBase;
            SIZE_T NumberOfNames = exportDirectoryEntry->NumberOfNames;
            SIZE_T NumberOfAddresses = exportDirectoryEntry->NumberOfFunctions;
            INT i;

            Module->Exports = 
                AllocatePool(sizeof(BOOT_MODULE_EXPORT) * NumberOfAddresses);

            Module->NumberOfExports = NumberOfAddresses;

            for (i = 0; i < NumberOfAddresses; i++)
            {
                PBOOT_MODULE_EXPORT Export = &Module->Exports[i];
                Export->ExportAddress = Addresses[i] + imageBase;
                Export->ExportAddressRva = Addresses[i];
                Export->ExportName = NULL;
                Export->ExportNameRva = NULL;
            }

            for (i = 0; i < NumberOfNames; i++)
            {
                PBOOT_MODULE_EXPORT Export;
                USHORT Ordinal = Ordinals[i];
                
                Export = &Module->Exports[Ordinal];
                Export->ExportNameRva = NamePointerTable[Ordinal];
                Export->ExportName = (PCHAR)(Export->ExportNameRva + imageBase);
                //Print(
                //    L"Linking name %a to ordinal %d (debased from %d)\n",
                //    Export->ExportName, Ordinal, Ordinals[i]);
            }

            RETURN_IF_ERROR(status);
        }
        else if (dataDirectoryIndex == IMAGE_DIRECTORY_ENTRY_IMPORT)
        {
            IMAGE_IMPORT_DIRECTORY_ENTRY* importDirectoryEntry = 
                (IMAGE_IMPORT_DIRECTORY_ENTRY*) dataDirectory;
            EFI_FILE_INFO* fileInfo;
            EFI_FILE_HANDLE fs;
            SIZE_T FileInfoSize = 64;
            
            status = file->Open(file, &fs, L"..", EFI_FILE_MODE_READ, 0);
            RETURN_IF_ERROR(status);

            do
            {
                FileInfoSize *= 2;
                status = fs->GetInfo(fs, &gEfiFileInfoGuid, &FileInfoSize, NULL);
            }
            while (status == EFI_BUFFER_TOO_SMALL);
            RETURN_IF_ERROR_DEBUG(status);

            fileInfo = AllocatePool(FileInfoSize);
            status = fs->GetInfo(fs, &gEfiFileInfoGuid, &FileInfoSize, fileInfo);
            RETURN_IF_ERROR_DEBUG(status);

            Print(L"file: %s %d\n", fileInfo->FileName, fileInfo->FileSize);
            FreePool(fileInfo);

            status = HandleImportDirectory(Module, fs, importDirectoryEntry);
            fs->Close(fs);
            RETURN_IF_ERROR(status);
        }
    }

    *outModule = Module;

    return status;
}

EFI_STATUS
QueryGraphicsInformation(
    BOOTDATA* bootdata)
{
    EFI_STATUS status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* graphicsProtocol;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* mode;

    status = gBootServices->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid, 
        NULL, 
        &graphicsProtocol);

    RETURN_IF_ERROR(status);

    mode = graphicsProtocol->Mode->Info;

    bootdata->dwHeight = mode->VerticalResolution;
    bootdata->dwWidth = mode->HorizontalResolution;
    bootdata->dwPixelsPerScanline = mode->PixelsPerScanLine;
    bootdata->pdwFramebuffer = (PDWORD)graphicsProtocol->Mode->FrameBufferBase;
    bootdata->pdwFramebufferEnd = 
        (PDWORD)((ULONG_PTR)graphicsProtocol->Mode->FrameBufferBase + 
                 (ULONG_PTR)graphicsProtocol->Mode->FrameBufferSize);

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

    PMMPFN_ENTRY pageFrameEntries;

    do
    {
        if (memoryMap != NULL)
            FreePool(memoryMap);

        status = gBootServices->AllocatePool(
            EfiLoaderData, 
            memoryMapSize, 
            &memoryMap);

        if (EFI_ERROR(status) || memoryMap == NULL)
            return EFI_ERROR(status) ? status : EFI_OUT_OF_RESOURCES;

        status = gBootServices->GetMemoryMap(
            &memoryMapSize,
            memoryMap, 
            &memoryMapKey, 
            &descriptorSize, 
            &descriptorVersion);
        
        memoryMapSize += descriptorSize;
    }
    while (status == EFI_BUFFER_TOO_SMALL);
    RETURN_IF_ERROR(status);

    currentDescriptor = memoryMap;
    while (currentDescriptor <= 
           (EFI_MEMORY_DESCRIPTOR*)((ULONG_PTR)memoryMap + memoryMapSize))
    {
        pages += currentDescriptor->NumberOfPages;
        currentDescriptor = (EFI_MEMORY_DESCRIPTOR*)
            ((ULONG_PTR)currentDescriptor + descriptorSize);
    }

    pageFrameEntries = AllocateZeroPool(pages * sizeof(MMPFN_ENTRY));

    for (int i = 0; i < pages; i++)
        pageFrameEntries[i].Flags = 1;

    currentDescriptor = memoryMap;
    while (currentDescriptor <= 
           (EFI_MEMORY_DESCRIPTOR*) ((ULONG_PTR) memoryMap + memoryMapSize))
    {
        UINTN relativePageIndex;
        ULONG_PTR flags = 1;
        
        switch (currentDescriptor->Type)
        {
            case EfiConventionalMemory:
                flags = 0;
                break;
            default:
                flags = 1;
                break;
        }

        for (relativePageIndex = 0; 
             relativePageIndex < currentDescriptor->NumberOfPages; 
             relativePageIndex++)
        {
            UINTN pageIndex = 
                currentDescriptor->PhysicalStart / PAGE_SIZE +
                relativePageIndex;

            pageFrameEntries[pageIndex].Flags = flags;
            
            if ((ULONG_PTR)pageIndex * PAGE_SIZE >= 
                    (ULONG_PTR)pageFrameEntries &&
                (ULONG_PTR)pageIndex * PAGE_SIZE <= 
                    ((ULONG_PTR) pageFrameEntries + pages))
            {
                pageFrameEntries[pageIndex].Flags = 1;
                continue;
            }
        }

        currentDescriptor = (EFI_MEMORY_DESCRIPTOR*) 
            ((ULONG_PTR) currentDescriptor + descriptorSize);
    }

    pageFrameEntries[0].Flags = 1;

    do
    {
        if (memoryMap != NULL)
            FreePool(memoryMap);

        status = gBootServices->AllocatePool(
            EfiLoaderData, 
            memoryMapSize, 
            &memoryMap);

        if (EFI_ERROR(status) || memoryMap == NULL)
            return EFI_ERROR(status) ? status : EFI_OUT_OF_RESOURCES;

        status = gBootServices->GetMemoryMap(
            &memoryMapSize, 
            memoryMap, 
            &memoryMapKey, 
            &descriptorSize, 
            &descriptorVersion);

        memoryMapSize += descriptorSize;
    }
    while (status == EFI_BUFFER_TOO_SMALL);
    RETURN_IF_ERROR(status);

    bootdata->mapKey = memoryMapKey;
    bootdata->NumberOfPageFrames = pages;
    bootdata->PageFrameDescriptorEntries = pageFrameEntries;

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI 
efi_main(
    EFI_HANDLE imageHandle, 
    EFI_SYSTEM_TABLE* systemTable)
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE_PROTOCOL* loadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystem;
    EFI_FILE_HANDLE root, kernelFile;
    BOOTDATA bootdata;
    VOID (*kernelEntrypoint)(BOOTDATA*);
    PLOADED_BOOT_MODULE module;

    gBootServices = systemTable->BootServices;
    InitializeLib(imageHandle, systemTable);

    InitializeListHead(&LoadedModules);

    status = gBootServices->HandleProtocol(
        imageHandle, 
        &gEfiLoadedImageProtocolGuid, 
        &loadedImage);
    RETURN_IF_ERROR_DEBUG(status);

    status = gBootServices->HandleProtocol(
        loadedImage->DeviceHandle, 
        &gEfiSimpleFileSystemProtocolGuid, 
        &filesystem);
    RETURN_IF_ERROR_DEBUG(status);

    status = filesystem->OpenVolume(
        filesystem, 
        &root);
    RETURN_IF_ERROR_DEBUG(status);

    status = root->Open(
        root, 
        &kernelFile, 
        (CHAR16*)wszKernelPath, 
        EFI_FILE_MODE_READ, 
        0);
    RETURN_IF_ERROR_DEBUG(status);

    status = LoadImage(
        kernelFile, 
        &module);
    RETURN_IF_ERROR_DEBUG(status);

    kernelEntrypoint = module->Entrypoint;
    Print(L"Kernel entrypoint %X, base %X\n", kernelEntrypoint, module->ImageBase);

    bootdata.dwKernelSize = module->ImageSize;
    bootdata.ExitBootServices = gBootServices->ExitBootServices;
    bootdata.pImageHandle = imageHandle;
    bootdata.MainKernelModule = *module;

    LibGetSystemConfigurationTable(&AcpiTableGuid, &bootdata.pRdsp);

    status = QueryGraphicsInformation(&bootdata);
    RETURN_IF_ERROR_DEBUG(status);

    status = QueryMemoryMap(&bootdata);
    RETURN_IF_ERROR_DEBUG(status);

    Print(L"Launching kernel\n");
    kernelEntrypoint(&bootdata);

    /* FIXME */
    // ClearListAndDestroyValues(&LoadedModules, DestroyLoadedModule);

    kernelFile->Close(kernelFile);
    root->Close(root);

    Print(L"Returning to EFI\n");
    return EFI_LOAD_ERROR;
}
