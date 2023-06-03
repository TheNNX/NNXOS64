#include <ldr.h>
#include <object.h>
#include <ntdebug.h>
#include <pool.h>
#include <file.h>
#include <nnxpe.h>

static POBJECT_TYPE   LdrModuleObjType;
static UNICODE_STRING LdrModuleObjTypeName = RTL_CONSTANT_STRING("Module");

typedef struct _MODULE_CREATION_DATA
{
    PCUNICODE_STRING Filename;
    PEPROCESS        Process;
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
    HANDLE hFile)
{
    IMAGE_DOS_HEADER DosHeader;
    IMAGE_PE_HEADER PeHeader;
    NTSTATUS Status;

    Status = LdrpFileReadHelper(hFile, &DosHeader, sizeof(DosHeader), 0);
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
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    /* TODO: WIP. */

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
LdrpLoadModuleFromFileToProcess(
    HANDLE hFile,
    PEPROCESS pProcess)
{
    ULONG_PTR CurrentAddressSpace;
    NTSTATUS Status;
    KIRQL Irql;

    /* Raise IRQL to prevent context switches while the other process'
     * address space is used. */
    KeRaiseIrql(DISPATCH_LEVEL, &Irql);
    
    /* Change the address space to the target process' address space. */
    CurrentAddressSpace = PagingGetAddressSpace();
    PagingSetAddressSpace(pProcess->Pcb.AddressSpacePhysicalPointer);

    Status = LdrpLoadFile(hFile);

    /* Restore the original address space and IRQL. */
    PagingSetAddressSpace(CurrentAddressSpace);
    KeLowerIrql(Irql);
    return Status;
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

    KeAcquireSpinLockAtDpcLevel(&Object->Lock);
    
    ObReferenceObjectUnsafe(Object);
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
LdrpModuleOnOpen(
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
    InitializeListHead(&Object->MemorySectionsHead);
    Object->Name = CreationData->Filename;

    InitializeObjectAttributes(
        &FileObjAttributes,
        (PUNICODE_STRING)CreationData->Filename,
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
        return Status;
    }

    Status = LdrpLoadModuleFromFileToProcess(
        Object->File, 
        CreationData->Process);
    if (!NT_SUCCESS(Status))
    {
        ObCloseHandle(Object->File, KernelMode);
        return Status;
    }

    Status = LdrpAddModuleToProcess(
        Object, 
        CreationData->Process);
    if (!NT_SUCCESS(Status))
    {
        ObCloseHandle(Object->File, KernelMode);
        return Status;
    }

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

    /* Add OnCreate and OnClose, which will open and close the file object
       and/or create mappings. */

    /* THIS SHOULD BE OnCreate I THINK!!! */
    LdrModuleObjType->OnOpen = LdrpModuleOnOpen;

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
LdrpLoadImage(
    PCUNICODE_STRING SearchPath,
    PCUNICODE_STRING ImageName,
    PHANDLE pOutMoudleHandle)
{
    PKMODULE pModule;
    OBJECT_ATTRIBUTES Attributes;
    MODULE_CREATION_DATA CreationData;
    NTSTATUS Status;
    HANDLE Handle;

    InitializeObjectAttributes(
        &Attributes, 
        NULL, 
        0, 
        NULL, 
        NULL);

    CreationData.Process = CONTAINING_RECORD(
        KeGetCurrentThread()->Process,
        EPROCESS, 
        Pcb);

    CreationData.Filename = ImageName;

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