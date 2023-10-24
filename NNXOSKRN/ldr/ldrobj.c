#include <ldr.h>
#include <object.h>
#include <ntdebug.h>
#include <pool.h>
#include <file.h>
#include <nnxpe.h>
#include <SimpleTextIO.h>

static POBJECT_TYPE   LdrModuleObjType;
static UNICODE_STRING LdrModuleObjTypeName = RTL_CONSTANT_STRING("Module");
static LIST_ENTRY     LdrLoadedModulesHead;
static KSPIN_LOCK     LdrLock;

typedef struct _DEPENDENCY_ENTRY
{
    LIST_ENTRY      Entry;
    PUNICODE_STRING Filename;
    PKMODULE        Module;
}DEPENDENCY_ENTRY, *PDEPENDENCY_ENTRY;

typedef struct _MODULE_CREATION_DATA
{
    PUNICODE_STRING Filename;
    PUNICODE_STRING SearchPath;
    PEPROCESS       Process;
}MODULE_CREATION_DATA, *PMODULE_CREATION_DATA;

NTSTATUS
NTAPI
LdrpFileReadHelper(
    HANDLE hFile,
    PVOID Buffer,
    ULONG Length,
    LONGLONG Offset)
{
    LARGE_INTEGER ReadOffset;
    ReadOffset.QuadPart = Offset;
    IO_STATUS_BLOCK StatusBlock;

    /* TODO: when a more complete implementation of NtReadFile is completed, 
     * this won't work - LdrpFileReadHelper is called exclusively by callers
     * running in IRQL >= PASSIVE_LEVEL. IRQL should be replaced by other
     * ways of preventing address space changing and other means of
     * synchronization. (Or use some other more internal filesystem access
     * API, this is, however, probably a bad idea - this code shouldn't really
     * run in high IRQL, maybe create some "Initialized" variable in KMODULE,
     * and make access to that locked behind a spinlock acquisition). */
    return NtReadFile(
        hFile,
        NULL,
        NULL,
        NULL,
        &StatusBlock,
        Buffer,
        Length,
        &ReadOffset,
        NULL);
}

/* TODO: pass some context regarding DLL search path or something like that. */
NTSTATUS
NTAPI
LdrpLoadFile(
    HANDLE hFile,
    PLOAD_IMAGE_INFO pLoadImageInfo)
{
    IMAGE_DOS_HEADER DosHeader;
    IMAGE_PE_HEADER PeHeader;
    ULONG DataDirSize;
    NTSTATUS Status;
    IMAGE_DATA_DIRECTORY DataDirectories[16];
    PIMAGE_SECTION_HEADER Sections;
    LONGLONG FilePointer;
    ULONG SizeOfSectionHeaders;

    FilePointer = 0;
    Status = LdrpFileReadHelper(
        hFile,
        &DosHeader,
        sizeof(DosHeader),
        FilePointer);
    FilePointer += sizeof(DosHeader);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    if (DosHeader.Signature != IMAGE_MZ_MAGIC)
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    Status = LdrpFileReadHelper(
        hFile, 
        &PeHeader, 
        sizeof(PeHeader), 
        DosHeader.e_lfanew);
    FilePointer = DosHeader.e_lfanew + sizeof(PeHeader);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }
    if (PeHeader.Signature != IMAGE_PE_MAGIC ||
        PeHeader.OptionalHeader.Signature != IMAGE_OPTIONAL_HEADER_NT64 ||
        PeHeader.FileHeader.Machine != IMAGE_MACHINE_X64)
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    DataDirSize =
        sizeof(*DataDirectories) *
        PeHeader.OptionalHeader.NumberOfDataDirectories;
    Status = LdrpFileReadHelper(
        hFile,
        DataDirectories,
        DataDirSize,
        FilePointer);
    FilePointer += DataDirSize;
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    SizeOfSectionHeaders = 
        PeHeader.FileHeader.NumberOfSections * sizeof(*Sections);

    Sections = ExAllocatePool(NonPagedPool, SizeOfSectionHeaders);
    if (Sections == NULL)
    {
        return STATUS_NO_MEMORY;
    }

    FilePointer = DosHeader.e_lfanew + sizeof(PeHeader) + DataDirSize;
    Status = LdrpFileReadHelper(
        hFile,
        Sections,
        SizeOfSectionHeaders,
        FilePointer);
    FilePointer += DataDirSize;
    if (!NT_SUCCESS(Status))
    {
        ExFreePool(Sections);
        return Status;
    }

    InitializeListHead(&pLoadImageInfo->MemorySectionsHead);
    InitializeListHead(&pLoadImageInfo->DependenciesHead);

    ExFreePool(Sections);
    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
LdrpLoadModuleFromFileToProcess(
    HANDLE hFile,
    PEPROCESS pProcess,
    PLOAD_IMAGE_INFO pLoadImageInfo)
{
    PADDRESS_SPACE CurrentAddressSpace;
    NTSTATUS Status;
    KIRQL Irql;

    /* Raise IRQL to prevent context switches while the other process'
     * address space is used. */
    KeRaiseIrql(DISPATCH_LEVEL, &Irql);
    
    /* Change the address space to the target process' address space. */
    CurrentAddressSpace = &KeGetCurrentProcess()->Pcb.AddressSpace;
    MmApplyAddressSpace(&pProcess->Pcb.AddressSpace);

    Status = LdrpLoadFile(hFile, pLoadImageInfo);

    /* Restore the original address space and IRQL. */
    MmApplyAddressSpace(CurrentAddressSpace);
    KeLowerIrql(Irql);
    return Status;
}

BOOLEAN
NTAPI
LdrpReferenceModuleByName(
    PCUNICODE_STRING ModuleFilename,
    PKMODULE* outpModule)
{
    PLIST_ENTRY Head = &LdrLoadedModulesHead;
    PLIST_ENTRY Current = Head->First;

    while (Head != Current)
    {
        PKMODULE Module = CONTAINING_RECORD(
            Current, 
            KMODULE, 
            LoadedModulesEntry);

        /* TODO! ModuleFilename should be an absolute path, and absolute 
         * paths should be compared AS PATHS and not strings (take into
         * account the potential object manager links between devices etc.) */
        if (RtlCompareUnicodeString(Module->Name, ModuleFilename, TRUE) == 0)
        {
            *outpModule = Module;
            return TRUE;
        }

        Current = Current->Next;
    }

    return FALSE;
}

NTSTATUS
NTAPI
LdrpHandleDependencies(
    PUNICODE_STRING SearchPath,
    PKMODULE pModule)
{
    PLIST_ENTRY Current = pModule->LoadImageInfo.DependenciesHead.First;
    PLIST_ENTRY Head = &pModule->LoadImageInfo.DependenciesHead;
    NTSTATUS Status;

    while (Current != Head)
    {
        PKMODULE pDependency = NULL;
        PDEPENDENCY_ENTRY Entry = CONTAINING_RECORD(
            Current,
            DEPENDENCY_ENTRY, 
            Entry);

        if (!LdrpReferenceModuleByName(Entry->Filename, &pDependency))
        {
            HANDLE hDependency;
            Status = LdrpLoadImage(SearchPath, Entry->Filename, &hDependency);
            if (!NT_SUCCESS(Status))
            {
                return Status;
            }

            Status = ObReferenceObjectByHandle(
                hDependency, 
                0, 
                LdrModuleObjType, 
                KernelMode, 
                &pDependency, 
                NULL);
            if (!NT_SUCCESS(Status))
            {
                return Status;
            }

            ObCloseHandle(hDependency, KernelMode);
        }

        Entry->Module = pDependency;
        Current = Current->Next;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
LdrpDereferenceDependencies(
    PKMODULE pModule)
{
    PLIST_ENTRY Current = pModule->LoadImageInfo.DependenciesHead.First;
    PLIST_ENTRY Head = &pModule->LoadImageInfo.DependenciesHead;
    NTSTATUS Status;

    while (Current != Head)
    {
        PKMODULE pDependency = NULL;
        PDEPENDENCY_ENTRY Entry = CONTAINING_RECORD(
            Current,
            DEPENDENCY_ENTRY,
            Entry);

        if (Entry->Module != NULL)
        {
            Status = ObDereferenceObject(Entry->Module);
            if (!NT_SUCCESS(Status))
            {
                return Status;
            }
        }

        Current = Current->Next;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
LdrpAddModuleToProcess(
    PKMODULE Object,
    PEPROCESS Process)
{
    KIRQL Irql;
    PKLOADED_MODULE_INSTANCE pInstance;

    KeAcquireSpinLock(&Process->Pcb.ProcessLock, &Irql);

    pInstance = ExAllocatePoolWithTag(
        NonPagedPool, 
        sizeof(*pInstance),
        'MODU');
    if (pInstance == NULL)
    {
        KeReleaseSpinLock(&Process->Pcb.ProcessLock, Irql);
        return STATUS_NO_MEMORY;
    }

    pInstance->Module = Object;
    pInstance->Process = Process;
    pInstance->References = 0;
    
    KeAcquireSpinLockAtDpcLevel(&Object->Lock);
    
    ObReferenceObject(Object);
    InsertTailList(&Object->InstanceHead, &pInstance->ModuleListEntry);
    InsertTailList(
        &Process->Pcb.ModuleInstanceHead, 
        &pInstance->ProcessListEntry);

    KeReleaseSpinLockFromDpcLevel(&Object->Lock);
    KeReleaseSpinLock(&Process->Pcb.ProcessLock, Irql);
    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
LdrpModuleOnDelete(
    PVOID SelfObject)
{
    PKMODULE pModule;

    ASSERT(SelfObject != NULL);
    pModule = (PKMODULE)SelfObject;

    PrintT("Deleting module - unimplemented.\n");

    /* TODO: Unimplemented. */
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS
NTAPI
LdrpModuleOnOpen(
    PVOID SelfObject,
    PVOID OptionalData)
{
    PKMODULE Object;
    NTSTATUS Status;
    PMODULE_CREATION_DATA CreationData;

    ASSERT(OptionalData != NULL && SelfObject != NULL);
    CreationData = (PMODULE_CREATION_DATA)OptionalData;
    Object = (PKMODULE)SelfObject;

    Status = LdrpAddModuleToProcess(
        Object,
        CreationData->Process);
    if (!NT_SUCCESS(Status))
    {
        ObCloseHandle(Object->File, KernelMode);
        return Status;
    }

    Status = LdrpHandleDependencies(CreationData->SearchPath, Object);
    if (!NT_SUCCESS(Status))
    {
        LdrpDereferenceDependencies(Object);
        ObCloseHandle(Object->File, KernelMode);
        return Status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
LdrpModuleOnCreate(
    PVOID SelfObject,
    PVOID OptionalData)
{
    PKMODULE Object;
    NTSTATUS Status;
    PMODULE_CREATION_DATA CreationData;
    IO_STATUS_BLOCK CompletionBlock;
    OBJECT_ATTRIBUTES FileObjAttributes;

    ASSERT(OptionalData != NULL && SelfObject != NULL);
    CreationData = (PMODULE_CREATION_DATA)OptionalData;
    Object = (PKMODULE)SelfObject;

    KeInitializeSpinLock(&Object->Lock);
    InitializeListHead(&Object->InstanceHead);
    Object->Name = CreationData->Filename;
    PrintT("Creating module\n");

    InitializeObjectAttributes(
        &FileObjAttributes,
        CreationData->Filename, /* FIXME: weird const cast? */
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);

    Status = NtCreateFile(
        &Object->File,
        FILE_GENERIC_READ,
        &FileObjAttributes,
        &CompletionBlock,
        0,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ,
        FILE_OPEN,
        0,
        NULL,
        0);
    if (!NT_SUCCESS(Status))
    {
        PrintT("NtCreateFile failed, status: %X\n", Status);
        return Status;
    }

    Status = LdrpLoadModuleFromFileToProcess(
        Object->File, 
        CreationData->Process,
        &Object->LoadImageInfo);
    if (!NT_SUCCESS(Status))
    {
        ObCloseHandle(Object->File, KernelMode);
        return Status;
    }

    Status = LdrpModuleOnOpen(SelfObject, OptionalData);
    if (!NT_SUCCESS(Status))
    {
        ObCloseHandle(Object->File, KernelMode);
        return Status;
    }

    ExInterlockedInsertTailList(
        &LdrLoadedModulesHead,
        &Object->LoadedModulesEntry,
        &LdrLock);

    return STATUS_SUCCESS;
}

/**
 * @brief Initializes the Module type used by the Ldr to store information
 * about the loaded modules.
 */
NTSTATUS
NTAPI
LdrpInitialize()
{
    NTSTATUS Status;

    Status = ObCreateType(
        &LdrModuleObjType, 
        &LdrModuleObjTypeName, 
        sizeof(KMODULE));
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    /* Add OnCreate and OnDelete, which will open and close the file object
       and/or create mappings. */
    LdrModuleObjType->OnCreate = LdrpModuleOnCreate;
    LdrModuleObjType->OnDelete = LdrpModuleOnDelete;
    LdrModuleObjType->OnOpen = LdrpModuleOnOpen;

    InitializeListHead(&LdrLoadedModulesHead);

    return STATUS_SUCCESS;
}

static UNICODE_STRING Dummy = RTL_CONSTANT_STRING(L"chuj");

NTSTATUS
NTAPI
LdrpLoadImage(
    PUNICODE_STRING SearchPath,
    PUNICODE_STRING ImageName,
    PHANDLE pOutMoudleHandle)
{
    PKMODULE pModule;
    OBJECT_ATTRIBUTES Attributes;
    MODULE_CREATION_DATA CreationData;
    NTSTATUS Status;
    HANDLE Handle;
    UNICODE_STRING Filename;
    PWSTR LastSlash;
    SIZE_T i;
    USHORT NewLen;

    Filename = *ImageName;
    NewLen = Filename.Length;

    /* Find the last slash in the path to get the filename. */
    LastSlash = Filename.Buffer;
    for (i = 0; i < Filename.Length / sizeof(*Filename.Buffer); i++)
    {
        if (Filename.Buffer[i] == L'\\' || Filename.Buffer[i] == L'/')
        {
            LastSlash = Filename.Buffer + i + 1;
            NewLen = 
                Filename.Length - (USHORT)(sizeof(*Filename.Buffer) * (i + 1));
        }
    }
    Filename.Buffer = LastSlash;
    Filename.Length = Filename.MaxLength = NewLen;

    InitializeObjectAttributes(
        &Attributes, 
        &Filename, 
        OBJ_OPENIF,
        NULL,
        NULL);

    CreationData.Process = CONTAINING_RECORD(
        KeGetCurrentThread()->Process,
        EPROCESS, 
        Pcb);

    CreationData.Filename = ImageName;
    CreationData.SearchPath = SearchPath;

    Status = ObCreateObject(
        &pModule, 
        0,
        KernelMode,
        &Attributes,
        LdrModuleObjType,
        &CreationData);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = ObCreateHandle(
        &Handle, 
        KernelMode,
        pModule);
    if (!NT_SUCCESS(Status))
    {
        return ObDereferenceObject(pModule);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
LdrpUnloadImage(
    HANDLE Module)
{
    return STATUS_NOT_SUPPORTED;
}

static UNICODE_STRING LdrpTestFileName = RTL_CONSTANT_STRING(L"EFI\\BOOT\\NNXOSKRN.EXE");

VOID
NTAPI
LdrTestThread(VOID)
{
    HANDLE hModule;
    NTSTATUS Status;

    PrintT("[" __FUNCTION__ "] Starting\n");

    for (int i = 0; i < 24; i++)
    {
        Status = LdrpLoadImage(NULL, &LdrpTestFileName, &hModule);
        ASSERTMSG("Ldr test failed\n", NT_SUCCESS(Status));
    }

    PrintT("[" __FUNCTION__ "] Done\n");
    while (1);
    PsExitThread(-1);
}