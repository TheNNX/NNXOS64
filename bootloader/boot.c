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
#include <physical_allocator.h>

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
    PLOADED_BOOT_MODULE* outModule);

const CHAR16* wszKernelPath = L"NNXOSKRN.exe";

static const CHAR16* PreloadPaths[] =
{
    L"NNXOSKRN.EXE",
    L"SMSS.EXE",
    L"APSTART.BIN",
    L"WIN32K.SYS"
};

EFI_BOOT_SERVICES* gBootServices;
LIST_ENTRY LoadedModules;

ULONG_PTR MaxKernelPhysAddress = 0ULL;
ULONG_PTR MinKernelPhysAddress = (ULONG_PTR)-1LL;
EFI_GUID gAcpi20TableGuid = ACPI_20_TABLE_GUID;

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

        if (Name != NULL && module->Name != NULL &&
            strcmpa(module->Name, Name) == 0)
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
        return EFI_NOT_FOUND;
    }

    if (ImportName == NULL)
    {
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
        CHAR* ModuleName = (CHAR*)(Current->NameRVA + ImageBase);
        Print(L"%a:\n", ModuleName);
        
        IMAGE_ILT_ENTRY64* CurrentImport = 
            (IMAGE_ILT_ENTRY64*)(Current->FirstThunkRVA + 
                                 ImageBase);

        /* OriginalFirstThunk and its thunk chain is repurposed in 
         * NNXOS bootmodule loading. Is is used to store pointers to
         * entries in the module export table of the module that exports
         * symbol referenced by given trunk. It is "pre-bound" in here.
         * Later, the kernel can use theese pointers to relocate the already
         * bound import entries. */
        IMAGE_ILT_ENTRY64* CurrentExportPreload =
            (IMAGE_ILT_ENTRY64*)(Current->OriginalFirstThunk +
                                 ImageBase);

        LOADED_BOOT_MODULE* Dependency = TryFindLoadedModule(ModuleName);

        if (Dependency == NULL)
        {
            Status = TryToLoadModule(ModuleName, SearchDirectory, &Dependency);
            if (Status)
            {
                return EFI_LOAD_ERROR;
            }
        }
        
        Print(L"Imports:\n");
        while (CurrentImport->AsNumber != 0)
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
                    (USHORT)CurrentImport->Ordinal,
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
                *((PULONG_PTR)CurrentExportPreload) = (ULONG_PTR)Export;
                *((PULONG_PTR)CurrentImport) = (ULONG_PTR)Export->ExportAddress;
            }

            CurrentImport++;
            CurrentExportPreload++;
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

static
EFI_STATUS
HandleExportDirectory(
    PLOADED_BOOT_MODULE Module,
    PIMAGE_EXPORT_DIRECTORY_ENTRY ExportDirectory)
{
    RVA* NamePointerTable = (RVA*)
        (ExportDirectory->AddressOfNamesRVA + Module->ImageBase);
    USHORT* Ordinals = (USHORT*)
        (ExportDirectory->AddressOfNameOrdinalsRVA + Module->ImageBase);
    /* TODO: handle forwarder RVAs */
    RVA* Addresses = (RVA*)
        (ExportDirectory->AddressOfFunctionsRVA + Module->ImageBase);
    USHORT OrdinalBase =
        ExportDirectory->OrdinalBase;
    SIZE_T NumberOfNames = ExportDirectory->NumberOfNames;
    SIZE_T NumberOfAddresses = ExportDirectory->NumberOfFunctions;
    INT i;

    for (i = 0; i < NumberOfAddresses; i++)
    {
        PBOOT_MODULE_EXPORT Export = &Module->Exports[i];
        Export->ExportAddress = Addresses[i] + Module->ImageBase;
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
        Export->ExportName = (PCHAR)(Export->ExportNameRva + Module->ImageBase);
    }

    return EFI_SUCCESS;
}

static
EFI_STATUS
HandleDataDirectories(
    LOADED_BOOT_MODULE* Module,
    EFI_FILE_HANDLE DllPathFile)
{
    EFI_STATUS Status;
    INT i;

    for (i = 0; i < Module->NumberOfDirectoryEntries; i++)
    {
        PVOID DataDirectory =
            (PVOID)(Module->DirectoryEntires[i].VirtualAddressRVA + 
                Module->ImageBase);

        if (Module->DirectoryEntires[i].Size == 0 ||
            Module->DirectoryEntires[i].VirtualAddressRVA == 0)
        {
            continue;
        }

        if (i == IMAGE_DIRECTORY_ENTRY_EXPORT)
        {
            PIMAGE_EXPORT_DIRECTORY_ENTRY ExportDirectory =
                (PIMAGE_EXPORT_DIRECTORY_ENTRY)DataDirectory;

            Status = HandleExportDirectory(Module, ExportDirectory);
            if (EFI_ERROR(Status))
            {
                return Status;
            }
        }
        else if (i == IMAGE_DIRECTORY_ENTRY_IMPORT)
        {
            PIMAGE_IMPORT_DIRECTORY_ENTRY ImportDirectoryEntry =
                (PIMAGE_IMPORT_DIRECTORY_ENTRY)DataDirectory;

            Status = HandleImportDirectory(
                Module, 
                DllPathFile, 
                ImportDirectoryEntry);

            if (EFI_ERROR(Status))
            {
                return Status;
            }
        }
    }

    return EFI_SUCCESS;
}

static
EFI_STATUS
LoadSectionsWithBase(
    EFI_FILE_HANDLE File,
    ULONG_PTR ImageBase,
    PIMAGE_PE_HEADER ImagePeHeader,
    PIMAGE_SECTION_HEADER Sections)
{
    EFI_STATUS Status;
    INT i;

    for (i = 0; i < ImagePeHeader->FileHeader.NumberOfSections; i++)
    {
        ULONG_PTR SectionSize, SectionStart;
        PIMAGE_SECTION_HEADER Section = &Sections[i];
        Status = File->SetPosition(File, Section->PointerToDataRVA);
        SectionSize = (UINTN)Section->SizeOfSection;

        if (EFI_ERROR(Status))
        {
            return Status;
        }

        SectionStart = Section->VirtualAddressRVA + ImageBase;

        ZeroMem(
            (PVOID)SectionStart,
            Section->VirtualSize);

        Status = File->Read(
            File,
            &SectionSize,
            (PVOID)(Section->VirtualAddressRVA + ImageBase));
        if (EFI_ERROR(Status))
        {
            return Status;
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS 
LoadImage(
    EFI_FILE_HANDLE File,
    PLOADED_BOOT_MODULE* outModule)
{
    IMAGE_DOS_HEADER ImageDosHeader;
    IMAGE_PE_HEADER ImagePeHeader;
    IMAGE_DATA_DIRECTORY DataDirectories[16];
    EFI_STATUS Status;
    PIMAGE_SECTION_HEADER Sections;
    SIZE_T DataDirSize, SizeOfSectionHeaders;
    SIZE_T Size;
    INTN NumberOfExports = 0;
    PLOADED_BOOT_MODULE Module;
    ULONG_PTR HighestSectionAddress;
    INTN TotalPagesToBeAllocated;
    INTN SectionHeaderPages, ImagePages, ModulePages, ExportDataPages;
    ULONG_PTR PhysAddr;
    EFI_FILE_HANDLE DllPathFile;

    Size = sizeof(ImageDosHeader);
    Status = File->Read(File, &Size, &ImageDosHeader);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = File->SetPosition(File, ImageDosHeader.e_lfanew);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Size = sizeof(ImagePeHeader);
    Status = File->Read(File, &Size, &ImagePeHeader);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    if (ImagePeHeader.Signature != IMAGE_PE_MAGIC)
    {
        return EFI_UNSUPPORTED;
    }
    
    if (ImagePeHeader.OptionalHeader.Signature != IMAGE_OPTIONAL_HEADER_NT64 ||
        ImagePeHeader.FileHeader.Machine != IMAGE_MACHINE_X64)
    {
        Print(L"%a: File specified is not a 64 bit executable\n", __FUNCDNAME__);
        return EFI_UNSUPPORTED;
    }

    DataDirSize =
        sizeof(*DataDirectories) *
        ImagePeHeader.OptionalHeader.NumberOfDataDirectories;
    Status = File->Read(File, &DataDirSize, DataDirectories);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    SizeOfSectionHeaders =
        ImagePeHeader.FileHeader.NumberOfSections *
        sizeof(IMAGE_SECTION_HEADER); 

    Sections = AllocatePool(SizeOfSectionHeaders);
    if (Sections == NULL)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = File->SetPosition(
        File,
        ImageDosHeader.e_lfanew +
        sizeof(ImagePeHeader) +
        DataDirSize);
    if (EFI_ERROR(Status))
    {
        FreePool(Sections);
        return Status;
    }

    Status = File->Read(
        File,
        &SizeOfSectionHeaders,
        Sections);
    if (EFI_ERROR(Status))
    {
        FreePool(Sections);
        return Status;
    }

    HighestSectionAddress = ImagePeHeader.OptionalHeader.SizeOfImage;

    /* Temporarily allocate a buffer for the sections, so the number of 
     * exports can be read. The number of exports is neccessary for the 
     * bootmodule info allocation, as it holds a table of exports in 
     * the module for easier relocation. */
    Status = gBootServices->AllocatePages(
        AllocateAnyPages,
        EfiLoaderData,
        HighestSectionAddress / PAGE_SIZE + 1,
        &PhysAddr);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = LoadSectionsWithBase(
        File,
        PhysAddr,
        &ImagePeHeader,
        Sections);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    /* Get the number of exported symbols. */
    if (ImagePeHeader.OptionalHeader.NumberOfDataDirectories - 1 >=
        IMAGE_DIRECTORY_ENTRY_EXPORT )
    {
        RVA ExportDirRva = 
            DataDirectories[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddressRVA;
        if (ExportDirRva)
        {
            PIMAGE_EXPORT_DIRECTORY_ENTRY ExportEntry =
                (PIMAGE_EXPORT_DIRECTORY_ENTRY)(ExportDirRva + PhysAddr);
                
            NumberOfExports = ExportEntry->NumberOfFunctions;
            Print(L"Exports %d\n", NumberOfExports);
        }
    }

    /* Free the temporary buffer. */
    Status = gBootServices->FreePages(
        PhysAddr, 
        HighestSectionAddress / PAGE_SIZE + 1);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    /* Calculate the size of the buffer for the bootmodule info. */

    /* Pages for the boot module info. */
    ModulePages = (sizeof(*Module) + PAGE_SIZE - 1) / PAGE_SIZE + 1;
    /* Pages for the module exports. */
    ExportDataPages =
        (sizeof(BOOT_MODULE_EXPORT) * NumberOfExports + PAGE_SIZE - 1) / PAGE_SIZE + 1;
    /* Pages for the section headers. */
    SectionHeaderPages = (SizeOfSectionHeaders + PAGE_SIZE - 1) / PAGE_SIZE + 1;
    /* Pages for the image sections. */
    ImagePages = (HighestSectionAddress + PAGE_SIZE - 1) / PAGE_SIZE + 1;

    TotalPagesToBeAllocated = 
        ModulePages + ExportDataPages + SectionHeaderPages + ImagePages;

    Status = gBootServices->AllocatePages(
        AllocateAnyPages, 
        EfiLoaderData, 
        TotalPagesToBeAllocated, 
        &PhysAddr);

    if (EFI_ERROR(Status))
    {
        FreePool(Sections);
        return Status;
    }

    if (PhysAddr < MinKernelPhysAddress)
    {
        MinKernelPhysAddress = PhysAddr;
    }

    if (PhysAddr + TotalPagesToBeAllocated * PAGE_SIZE > MaxKernelPhysAddress)
    {
        MaxKernelPhysAddress = PhysAddr + TotalPagesToBeAllocated * PAGE_SIZE;
    }

    Module = (PLOADED_BOOT_MODULE)PhysAddr;
    Module->Exports = 
        (PBOOT_MODULE_EXPORT)(PhysAddr + ModulePages * PAGE_SIZE);
    Module->ImageBase =
        PhysAddr +
        (ModulePages + ExportDataPages + SectionHeaderPages) * PAGE_SIZE;
    Module->Entrypoint =
        ImagePeHeader.OptionalHeader.EntrypointRVA + Module->ImageBase;
    Module->ImageSize =
        ImagePeHeader.OptionalHeader.SizeOfImage;
    Module->NumberOfExports = NumberOfExports;
    Module->NumberOfSectionHeaders =
        ImagePeHeader.FileHeader.NumberOfSections;
    Module->SectionHeaders = 
        (PIMAGE_SECTION_HEADER)(PhysAddr + (ModulePages + ExportDataPages) * PAGE_SIZE);
    Module->NumberOfDirectoryEntries = 
        ImagePeHeader.OptionalHeader.NumberOfDataDirectories;
    Module->Name = NULL;
    *outModule = Module;

    CopyMem(
        (void*)Module->DirectoryEntires,
        DataDirectories,
        DataDirSize);

    CopyMem(
        (void*)Module->SectionHeaders,
        Sections,
        SizeOfSectionHeaders);
    
    FreePool(Sections);
    Sections = Module->SectionHeaders;
    InsertTailList(&LoadedModules, &Module->ListEntry);

    Status = LoadSectionsWithBase(
        File, 
        Module->ImageBase,
        &ImagePeHeader,
        Sections);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = File->Open(File, &DllPathFile, L"..", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status))
    {
        return Status;
    }
    HandleDataDirectories(Module, DllPathFile);
    DllPathFile->Close(DllPathFile);

    return EFI_SUCCESS;
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

/* TODO: Make less ugly */
EFI_STATUS
QueryMemoryMap(
    BOOTDATA* bootdata,
    UINTN* pOutMapKey)
{
    EFI_STATUS status;
    UINTN memoryMapSize = 0, memoryMapKey, descriptorSize;
    EFI_MEMORY_DESCRIPTOR* memoryMap = NULL;
    UINT32 descriptorVersion;
    EFI_MEMORY_DESCRIPTOR* currentDescriptor;
    UINTN pages = 0;

    PMMPFN_ENTRY pageFrameEntries;

    memoryMapSize = 1;

    do
    {
        if (memoryMap != NULL)
        {
            Print(L"Trying %d\n", memoryMapSize);
            FreePool(memoryMap);
        }

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

        if (status == EFI_BUFFER_TOO_SMALL)
        {
            memoryMapSize *= 3;
            memoryMapSize /= 2;
        }
    }
    while (status == EFI_BUFFER_TOO_SMALL);
    RETURN_IF_ERROR(status);

    Print(L"Total memmap %d\n", memoryMapSize);

    currentDescriptor = memoryMap;
    while (currentDescriptor < 
           (EFI_MEMORY_DESCRIPTOR*)((ULONG_PTR)memoryMap + memoryMapSize))
    {
        if (currentDescriptor->Type == EfiConventionalMemory &&
            pages < currentDescriptor->NumberOfPages + (
            currentDescriptor->PhysicalStart + PAGE_SIZE - 1) / PAGE_SIZE)
        {
            pages = currentDescriptor->NumberOfPages + (
                currentDescriptor->PhysicalStart + PAGE_SIZE - 1) / PAGE_SIZE;
        }

        currentDescriptor = (EFI_MEMORY_DESCRIPTOR*)
            ((ULONG_PTR)currentDescriptor + descriptorSize);
    }

    Print(L"Found %d megabytes free, pages: %d\n", pages * PAGE_SIZE / 1024 / 1024, pages);
    pageFrameEntries = AllocateZeroPool(pages * sizeof(MMPFN_ENTRY));
    Print(L"Alloc result %X\n", pageFrameEntries);

    for (int i = 0; i < pages; i++)
        pageFrameEntries[i].Flags = 1;

    currentDescriptor = memoryMap;
    while (currentDescriptor < 
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

            if (pageIndex >= pages)
                break;

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
        {
            FreePool(memoryMap);
        }

        status = gBootServices->AllocatePool(
            EfiLoaderData, 
            memoryMapSize, 
            &memoryMap);

        if (EFI_ERROR(status))
        {
            return status;
        }

        status = gBootServices->GetMemoryMap(
            &memoryMapSize, 
            memoryMap, 
            &memoryMapKey, 
            &descriptorSize, 
            &descriptorVersion);

        memoryMapSize *= 3;
        memoryMapSize /= 2;
    }
    while (status == EFI_BUFFER_TOO_SMALL);
    RETURN_IF_ERROR(status);

    *pOutMapKey = memoryMapKey;
    bootdata->NumberOfPageFrames = pages;
    bootdata->PageFrameDescriptorEntries = pageFrameEntries;

    return EFI_SUCCESS;
}

VOID
RundownLoadedFiles(
    PBOOTDATA pBootdata)
{
    PLIST_ENTRY pCurrent, pHead;
    PPRELOADED_FILE pPreloaded;

    pHead = &pBootdata->PreloadedFiles;
    pCurrent = pHead->First;

    while (pCurrent != pHead)
    {
        pPreloaded = CONTAINING_RECORD(pCurrent, PRELOADED_FILE, Entry);

        if (pPreloaded->Data != NULL)
        {
            gBS->FreePool(pPreloaded->Data);
        }

        pCurrent = pCurrent->Next;
        gBS->FreePool(pPreloaded);
    }

    InitializeListHead(pHead);
}

EFI_STATUS
PreloadBootFiles(
    EFI_FILE_HANDLE filesystem,
    PBOOTDATA pBootdata)
{
    SIZE_T i;
    PPRELOADED_FILE pCurrent;
    EFI_STATUS status;
    EFI_FILE_HANDLE hFile;
    EFI_FILE_INFO* fileInfo;
    UINTN bufferSize;

    for (i = 0; i < sizeof(PreloadPaths) / sizeof(*PreloadPaths); i++)
    {
        status = gBS->AllocatePool(EfiLoaderData, sizeof(*pCurrent), &pCurrent);
        if (status != EFI_SUCCESS)
        {
            RundownLoadedFiles(pBootdata);
            return status;
        }

        InsertTailList(&pBootdata->PreloadedFiles, &pCurrent->Entry);
        pCurrent->Filesize = 0;
        pCurrent->Data = NULL;
        RtStpCpy(pCurrent->Name, PreloadPaths[i]);

        status = filesystem->Open(
            filesystem,
            &hFile,
            (CHAR16*)PreloadPaths[i],
            EFI_FILE_MODE_READ,
            0);
        if (status != EFI_SUCCESS)
        {
            RundownLoadedFiles(pBootdata);
            return status;
        }

        bufferSize = sizeof(EFI_FILE_INFO);
        do
        {
            status = gBS->AllocatePool(EfiLoaderData, bufferSize, &fileInfo);
            if (status != EFI_SUCCESS)
            {
                break;
            }
            
            status = hFile->GetInfo(hFile, &gEfiFileInfoGuid, &bufferSize, fileInfo);
        }
        while (status == EFI_BUFFER_TOO_SMALL);

        if (status != EFI_SUCCESS)
        {
            hFile->Close(hFile);
            RemoveEntryList(&pCurrent->Entry);
            gBS->FreePool(pCurrent);
            Print(L"Failed preloading file '%s', %r, %d\n", PreloadPaths[i], status, bufferSize);
            continue;
        }

        pCurrent->Filesize = fileInfo->FileSize;
        gBS->FreePool(fileInfo);

        status = gBS->AllocatePool(EfiLoaderData, pCurrent->Filesize, &pCurrent->Data);
        if (status != EFI_SUCCESS)
        {
            RundownLoadedFiles(pBootdata);
            hFile->Close(hFile);
            return status;
        }

        bufferSize = pCurrent->Filesize;
        status = hFile->Read(hFile, &bufferSize, pCurrent->Data);
        if (status != EFI_SUCCESS)
        {
            hFile->Close(hFile);
            RemoveEntryList(&pCurrent->Entry);
            gBS->FreePool(pCurrent->Data);
            gBS->FreePool(pCurrent);
            Print(L"Failed preloading file '%s'\n", PreloadPaths[i]);
            continue;
        }

        hFile->Close(hFile);
    }

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
    UINTN MapKey;

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

    kernelEntrypoint = (void(*)(BOOTDATA*))module->Entrypoint;
    Print(
        L"Kernel entrypoint %X, base %X\n",
        kernelEntrypoint, 
        module->ImageBase);
        
    bootdata.dwKernelSize = module->ImageSize;
    bootdata.pImageHandle = imageHandle;
    bootdata.MainKernelModule = module;
    bootdata.ModuleHead = LoadedModules;
    bootdata.ModuleHead.Last->Next = &bootdata.ModuleHead;
    bootdata.ModuleHead.First->Prev = &bootdata.ModuleHead;
    InitializeListHead(&bootdata.PreloadedFiles);
    bootdata.MaxKernelPhysAddress = MaxKernelPhysAddress;
    bootdata.MinKernelPhysAddress = MinKernelPhysAddress;
    LibGetSystemConfigurationTable(&gAcpi20TableGuid, &bootdata.pRdsp);
    
#ifdef INACCESIBLE_BOOT_DEVICE
    status = PreloadBootFiles(root, &bootdata);
    if (status != EFI_SUCCESS)
    {
        Print(L"%r", status);
        return status;
    }

    /*
    PPRELOADED_FILE pCurrent = (PPRELOADED_FILE)bootdata.PreloadedFiles.First;
    while (pCurrent != (PPRELOADED_FILE)&bootdata.PreloadedFiles)
    {
        Print(L"File %s, first 16 characters:\n", pCurrent->Name);
        for (int i = 0; i < 16; i++)
        {
            Print(L"'%c', ", ((PCHAR)pCurrent->Data)[i]);
        }

        pCurrent = (PPRELOADED_FILE)pCurrent->Entry.Next;
    }

    while (1);
    */
#endif

    gBootServices->SetWatchdogTimer(0, 0, 0, NULL);

    status = QueryGraphicsInformation(&bootdata);
    RETURN_IF_ERROR_DEBUG(status);
    status = QueryMemoryMap(&bootdata, &MapKey);
    RETURN_IF_ERROR_DEBUG(status);

    for (int i = 0; i < 1024 * 768; i++)
    {
        if (i % 4 == 0)
            bootdata.pdwFramebuffer[i] = 0xFF00FFFF;
    }

    status = gBootServices->ExitBootServices(imageHandle, MapKey);
    if (status != EFI_SUCCESS)
    {
        kernelFile->Close(kernelFile);
        root->Close(root);
        Print(L"%r", status);
        return status;
    }

    kernelEntrypoint(&bootdata);

    /* FIXME */
    // ClearListAndDestroyValues(&LoadedModules, DestroyLoadedModule);

    kernelFile->Close(kernelFile);
    root->Close(root);

    Print(L"Returning to EFI\n");
    return EFI_LOAD_ERROR;
}
