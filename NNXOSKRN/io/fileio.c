#include <nnxtype.h>
#include <dispatcher.h>
#include <object.h>
#include <file.h>

/**
 * @brief This structure is used to hold data in IoFileObjectType instances.
 */
typedef struct _SHARED_FILE_OBJECT
{
    LIST_ENTRY Instances;
    BOOLEAN    bIsDirectory;
    /* TODO: ERESOURCE ShareAccess; */
}SHARED_FILE_OBJECT, *PSHARED_FILE_OBJECT;

typedef struct _FILE_OPEN_INSTANCE_INIT_DATA
{
    PSHARED_FILE_OBJECT SharedFileObject;
    ACCESS_MASK         AccessMask;
}FILE_OPEN_INSTANCE_INIT_DATA, *PFILE_OPEN_INSTANCE_INIT_DATA;

typedef struct _FILE_OPEN_INSTANCE_OBJECT
{
    LIST_ENTRY          ListEntry;
    PSHARED_FILE_OBJECT SharedFileObject;
}FILE_OPEN_INSTANCE_OBJECT, *PFILE_OPEN_INSTANCE_OBJECT;

typedef struct _SHARED_FILE_OBJECT_INIT_DATA
{
    ULONG FileAttributes;
    ULONG ShareAccess;
    ULONG CreateDisposition;
    ULONG CreateOptions;
    PVOID ExtAttributesBuffer;
    ULONG EaBufferSize;
}SHARED_FILE_OBJECT_INIT_DATA, *PSHARED_FILE_OBJECT_INIT_DATA;

POBJECT_TYPE IoFileObjectType;
POBJECT_TYPE IoFileOpenInstanceObjectType;

static UNICODE_STRING IoFileTypeName =
    RTL_CONSTANT_STRING(L"File");

static UNICODE_STRING IoFileOpenInstanceObjectTypeName =
    RTL_CONSTANT_STRING(L"FileOpenInstance");

static
NTSTATUS
NTAPI
InstanceOnCreate(
    PVOID SelfObject,
    PVOID InitData)
{
    PSHARED_FILE_OBJECT pSharedObject;
    PFILE_OPEN_INSTANCE_OBJECT pInstance;

    PFILE_OPEN_INSTANCE_INIT_DATA CreateData =
        (PFILE_OPEN_INSTANCE_INIT_DATA) InitData;

    pInstance = (PFILE_OPEN_INSTANCE_OBJECT)SelfObject;
    pSharedObject = CreateData->SharedFileObject;
    InsertTailList(&pSharedObject->Instances, &pInstance->ListEntry);

    return STATUS_SUCCESS;
}

static
NTSTATUS
NTAPI
InstanceOnClose(
    PVOID Self)
{
    PFILE_OPEN_INSTANCE_OBJECT Object = (PFILE_OPEN_INSTANCE_OBJECT) Self;

    RemoveEntryList(&Object->ListEntry);
    ObDereferenceObject((PVOID)Object->SharedFileObject);

    return STATUS_SUCCESS;
}

static
NTSTATUS
NTAPI
SharedObjOnOpen(
    PVOID Self,
    PVOID OpenData)
{
    PSHARED_FILE_OBJECT SharedPbject = (PSHARED_FILE_OBJECT)Self;
    PSHARED_FILE_OBJECT_INIT_DATA InitData =
        (PSHARED_FILE_OBJECT_INIT_DATA)OpenData;

    /* TODO. */

    return STATUS_NOT_SUPPORTED;
}

/**
 * @brief Initializes the object manager types used by the file I/O functions,
 * namely IoFileObjectType. 
 */
NTSTATUS
NTAPI
NtFileObjInit()
{
    NTSTATUS Status;

    Status = ObCreateType(
        &IoFileObjectType, 
        &IoFileTypeName, 
        sizeof(SHARED_FILE_OBJECT));
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    IoFileObjectType->OnOpen = SharedObjOnOpen;

    Status = ObCreateType(
        &IoFileOpenInstanceObjectType,
        &IoFileOpenInstanceObjectTypeName,
        sizeof(FILE_OPEN_INSTANCE_OBJECT));
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    IoFileOpenInstanceObjectType->OnCreate = InstanceOnCreate;
    IoFileOpenInstanceObjectType->OnClose = InstanceOnClose;

    return STATUS_SUCCESS;
}

static
NTSTATUS
NtCreateInstanceFileObjectFromShared(
    PSHARED_FILE_OBJECT pSharedFile,
    POBJECT_ATTRIBUTES pInObjectAttributes,
    ACCESS_MASK AccessMask,
    KPROCESSOR_MODE Mode,
    PFILE_OPEN_INSTANCE_OBJECT* pOutFileOpenInstance)
{
    PFILE_OPEN_INSTANCE_OBJECT Instance;
    NTSTATUS Status;

    Status = ObReferenceObject((PVOID)pSharedFile);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }
    
    Status = ObCreateObject(
        &Instance,
        AccessMask,
        Mode,
        pInObjectAttributes,
        IoFileOpenInstanceObjectType,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        ObDereferenceObject((PVOID)pSharedFile);
        return Status;
    }

    return STATUS_SUCCESS;
}


NTSTATUS
NTAPI
NtCreateFile(
    PHANDLE pOutHandle,
    ACCESS_MASK DesiredAccessMask,
    POBJECT_ATTRIBUTES pInObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    PLARGE_INTEGER AllocationSize,
    ULONG FileAttributes,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    PVOID ExtAttributesBuffer,
    ULONG EaBufferSize)
{
    NTSTATUS Status;
    PSHARED_FILE_OBJECT FileObject;
    OBJECT_ATTRIBUTES ObjectAttributes;
    PFILE_OPEN_INSTANCE_OBJECT Instance;
    SHARED_FILE_OBJECT_INIT_DATA CreateData;
    HANDLE Handle;
    
    /* Create with OBJ_OPENIF, so if the file is already open, we can create 
     * a handle to that instance. */
    ObjectAttributes = *pInObjectAttributes;
    ObjectAttributes.Attributes |= OBJ_OPENIF | OBJ_OPENLINK;

    /* Initialize the creation data. */
    CreateData.CreateDisposition = CreateDisposition;
    CreateData.CreateOptions = CreateOptions;
    CreateData.EaBufferSize = EaBufferSize;
    CreateData.ExtAttributesBuffer = ExtAttributesBuffer;
    CreateData.FileAttributes = FileAttributes;
    CreateData.ShareAccess = ShareAccess;

    /* Create the named, shared file object. */
    /* The OnOpen method of the IoFileObjectType handles the calls to the 
     * appropriate filesystem driver to open or create the file. It also
     * performs the shared access checks to see if the file being open
     * can be open for writing for example (TODO: implement ERESOURCE or
     * something similar). */
    Status = ObCreateObject(
        &FileObject,
        0,
        KernelMode,
        &ObjectAttributes, /* TODO: handle attributes in a better way. */
        IoFileObjectType,
        &CreateData);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }
    
    InitializeObjectAttributes(
        &ObjectAttributes,
        NULL, 
        0, 
        NULL, 
        NULL);

    /* If file is already open with FILE_SHARE_WRITE,
     * and we require write access. */
    /* TODO. */
    Status = NtCreateInstanceFileObjectFromShared(
        FileObject,
        &ObjectAttributes,
        DesiredAccessMask,
        KernelMode,
        &Instance);
    if (!NT_SUCCESS(Status))
    {
        ObDereferenceObject(FileObject);
        return Status;
    }

    Status = ObCreateHandle(&Handle, KernelMode, Instance);
    if (!NT_SUCCESS(Status))
    {
        ObDereferenceObject(Instance);
        ObDereferenceObject(FileObject);
        return Status;
    }

    ObDereferenceObject(FileObject);
    return STATUS_SUCCESS;
}