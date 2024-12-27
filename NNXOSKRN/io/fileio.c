/**
 * Copyright 2023 Marcin Jab³oñski
 * Licensed under the GNU LGPL 3.0.
 * 
 * fileio.c - Implements NT file input/output routines. 
 */

/* TODO: Rewrite NNXOS64 VFS code - UNICODE_STRING support, smarter device 
   detection (or any device detection at all, so far there's none), etc. 
   
   This file doesn't make a lot of use of the VFS code, but some VFS_MAX_PATHs
   should be probably removed, and some smarter dynamic size strings backed
   by the RTL maybe should be used. */

#include <nnxtype.h>
#include <dispatcher.h>
#include <object.h>
#include <file.h>
#include <vfs.h>
#include <ntdebug.h>

/**
 * @brief This object is used to hold general filesystem information about the 
 * file and is a global named object. Its existence signifies that the file it 
 * represents is open by one or more process. To get information about the open
 * instances, enumerate the Instances list, each entry is an entry to 
 * a FILE_OPEN_INSTANCE_OBJECT. 
 */
typedef struct _SHARED_FILE_OBJECT
{
    LIST_ENTRY Instances;
    /* TODO: ERESOURCE ShareAccess; */

    PVFS       pFileSystem;
    PVFS_FILE  pFile;
    
    HANDLE     hImageSection;
}SHARED_FILE_OBJECT, *PSHARED_FILE_OBJECT;

/**
 * @brief This type of object is used to store data about a particular open 
 * instance of a file. The instance object is unnamed - the only way to access 
 * it is the handle returned by this function, or by enumerating the list of 
 * instance objects in the shared object.
 */
typedef struct _FILE_OPEN_INSTANCE_OBJECT
{
    LIST_ENTRY          ListEntry;
    PSHARED_FILE_OBJECT SharedFileObject;
}FILE_OPEN_INSTANCE_OBJECT, *PFILE_OPEN_INSTANCE_OBJECT;

typedef struct _FILE_OPEN_INSTANCE_INIT_DATA
{
    PSHARED_FILE_OBJECT SharedFileObject;
    ACCESS_MASK         AccessMask;
}FILE_OPEN_INSTANCE_INIT_DATA, *PFILE_OPEN_INSTANCE_INIT_DATA;

typedef struct _SHARED_FILE_OBJECT_INIT_DATA
{
    ULONG FileAttributes;
    ULONG ShareAccess;
    ULONG CreateDisposition;
    ULONG CreateOptions;
    PVOID ExtAttributesBuffer;
    ULONG EaBufferSize;
    PUNICODE_STRING Filepath;
}SHARED_FILE_OBJECT_INIT_DATA, *PSHARED_FILE_OBJECT_INIT_DATA;

POBJECT_TYPE IoFileObjectType;
POBJECT_TYPE IoFileOpenInstanceObjectType;

static UNICODE_STRING IoFileTypeName =
    RTL_CONSTANT_STRING(L"File");

static UNICODE_STRING IoFileOpenInstanceObjectTypeName =
    RTL_CONSTANT_STRING(L"FileOpenInstance");

static UNICODE_STRING PathSeparatorString =
    RTL_CONSTANT_STRING(L"\\");

static
NTSTATUS
NTAPI
InstanceOnCreate(
    PVOID SelfObject,
    PVOID InitData)
{
    PFILE_OPEN_INSTANCE_OBJECT pInstance;

    PFILE_OPEN_INSTANCE_INIT_DATA CreateData =
        (PFILE_OPEN_INSTANCE_INIT_DATA) InitData;

    pInstance = (PFILE_OPEN_INSTANCE_OBJECT)SelfObject;
    pInstance->SharedFileObject = CreateData->SharedFileObject;

    InsertTailList(
        &pInstance->SharedFileObject->Instances, 
        &pInstance->ListEntry);

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
SharedObjOnCreate(
    PVOID Self,
    PVOID OpenData)
{
    PSHARED_FILE_OBJECT SharedObject = (PSHARED_FILE_OBJECT)Self;
    PSHARED_FILE_OBJECT_INIT_DATA InitData =
        (PSHARED_FILE_OBJECT_INIT_DATA)OpenData;
    
    SIZE_T i;
    PVFS pFilesystem;
    CHAR asciiPath[VFS_MAX_PATH] = { 0 };

    /* TODO: handle CreateDisposition (note to self: to do so, SharedObjOnOpen 
       is probably neccessary) */

    /* TODO: device selection. */
    pFilesystem = VfsGetSystemVfs();
    SharedObject->pFileSystem = pFilesystem;

    SharedObject->hImageSection = NULL;

    for (i = 0; i < VFS_MAX_PATH && i < InitData->Filepath->Length / 2; i++)
    {
        asciiPath[i] = (CHAR)(InitData->Filepath->Buffer[i] & 0xFF);
    }

    SharedObject->pFile =
        pFilesystem->Functions.OpenFile(pFilesystem, asciiPath);
    if (SharedObject->pFile == NULL)
    {
        return STATUS_NO_SUCH_FILE;
    }

    InitializeListHead(&SharedObject->Instances);
    return STATUS_SUCCESS;
}

/**
 * @brief Initializes the object manager types used by the file I/O functions,
 * namely IoFileObjectType. 
 */
NTSTATUS
NTAPI
NtFileObjInit(VOID)
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

    IoFileObjectType->OnCreate = SharedObjOnCreate;

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
    FILE_OPEN_INSTANCE_INIT_DATA CreateData;
    NTSTATUS Status;

    Status = ObReferenceObject((PVOID)pSharedFile);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    CreateData.AccessMask = AccessMask;
    CreateData.SharedFileObject = pSharedFile;

    Status = ObCreateObject(
        &Instance,
        AccessMask,
        Mode,
        pInObjectAttributes,
        IoFileOpenInstanceObjectType,
        (PVOID)&CreateData);

    if (!NT_SUCCESS(Status))
    {
        ObDereferenceObject((PVOID)pSharedFile);
        return Status;
    }

    *pOutFileOpenInstance = Instance;
    return STATUS_SUCCESS;
}

static
SIZE_T
StrLen(const char* c)
{
    SIZE_T Result = 0;
    while (*c++)
    {
        Result++;
    }
    return Result;
}

static
NTSTATUS
GetPathFromParent(
    HANDLE hParent, 
    PUNICODE_STRING pResult,
    KPROCESSOR_MODE Mode)
{
    PSHARED_FILE_OBJECT pParent;
    NTSTATUS Status;
    SIZE_T i;

    Status = ObReferenceObjectByHandle(
        hParent,
        0,
        IoFileObjectType,
        Mode,
        (PVOID)&pParent,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    if (StrLen(pParent->pFile->Path) > pResult->MaxLength / 2)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    for (i = 0; i < StrLen(pParent->pFile->Path); i++)
    {
        pResult->Buffer[i] = (WCHAR)pParent->pFile->Path[i];
    }

    return STATUS_SUCCESS;
}

/* TODO: PreviousMode checks. */
/* TODO: pInObjectAttributes->Root == NULL shouldn't be accepted as a valid 
   attribute - it should be changed to some sort of a Device/Filesystem/Driver
   ObMgr object that implements appropriate functions to access its children
   (files). */
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
    WCHAR PathBuffer[VFS_MAX_PATH];
    UNICODE_STRING Path = 
    {
        .Length = 0,
        .MaxLength = sizeof(PathBuffer),
        .Buffer = PathBuffer
    };
    
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
    if (ObjectAttributes.Root != NULL)
    {
        Status = GetPathFromParent(ObjectAttributes.Root, &Path, KernelMode);
        if (!NT_SUCCESS(Status))
        {
            return Status;
        }

        Status = RtlUnicodeStringCat(&Path, &PathSeparatorString);
        if (!NT_SUCCESS(Status))
        {
            return Status;
        }
    }

    Status = RtlUnicodeStringCat(&Path, ObjectAttributes.ObjectName);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    CreateData.Filepath = &Path;

    /* Create the named, shared file object. */
    /* The OnOpen and the OnCreate methods of the IoFileObjectType handle the 
     * calls to the appropriate filesystem driver to open or create the file. It
     * also performs the shared access checks to see if the file being open can 
     * be open for writing for example (TODO: implement ERESOURCE or something 
     * similar). */
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

    /* Create an instance object. */
    InitializeObjectAttributes(
        &ObjectAttributes,
        NULL, 
        0, 
        NULL, 
        NULL);

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

    Status = ObCreateHandle(&Handle, KernelMode, TRUE, Instance);
    if (!NT_SUCCESS(Status))
    {
        ObDereferenceObject(Instance);
        ObDereferenceObject(FileObject);
        return Status;
    }

    *pOutHandle = Handle;
    /* Dereference the file object, as it is referenced twice. ObCreateObject
     * creates an object with one reference, creating the handle creates the
     * second reference. */
    ObDereferenceObject(FileObject);
    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
NtReadFile(
    HANDLE hFile,
    HANDLE hEvent,
    /* Reserved */
    PVOID pApcRoutine,
    /* Reserved */
    PVOID pApcContext,
    PIO_STATUS_BLOCK pStatusBlock,
    PVOID Buffer,
    ULONG Length,
    PLARGE_INTEGER ByteOffset,
    PULONG Key)
{
    PVFS_FILE pFile;
    NTSTATUS Status;
    PVFS pFilesystem;
    PFILE_OPEN_INSTANCE_OBJECT pInstance;
    VFS_STATUS VfsStatus;

    Status = ObReferenceObjectByHandle(
        hFile,
        0,
        IoFileOpenInstanceObjectType,
        KernelMode,
        &pInstance,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }
    
    pFilesystem = pInstance->SharedFileObject->pFileSystem;
    pFile = pInstance->SharedFileObject->pFile;

    /* TODO: ByteOffset can be NULL or "invalid" if file is created with 
       FILE_SYNCHRONOUS_IO_ALERT or FILE_SYNCHRONOUS_IO_NONALERT. */
    pFile->FilePointer = ByteOffset->QuadPart;
    VfsStatus = pFilesystem->Functions.ReadFile(pFile, Length, Buffer);
    pStatusBlock->Information = pFile->FilePointer - ByteOffset->QuadPart;

    if (VfsStatus == VFS_ERR_EOF)
    {
        ObDereferenceObject(pInstance);
        return STATUS_END_OF_FILE;
    }    

    pStatusBlock->Status = STATUS_SUCCESS;

    Status = ObDereferenceObject(pInstance);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
NnxGetNtFileSize(HANDLE hFile, PLARGE_INTEGER outSize)
{
    PVFS_FILE pFile;
    NTSTATUS Status;
    PVFS pFilesystem;
    PFILE_OPEN_INSTANCE_OBJECT pInstance;

    Status = ObReferenceObjectByHandle(
        hFile,
        0,
        IoFileOpenInstanceObjectType,
        KernelMode,
        &pInstance,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    pFilesystem = pInstance->SharedFileObject->pFileSystem;
    pFile = pInstance->SharedFileObject->pFile;

    outSize->QuadPart = pFile->FileSize;

    Status = ObDereferenceObject(pInstance);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
NnxGetImageSection(
    HANDLE hFile,
    PHANDLE outSection)
{
    NTSTATUS Status;
    PFILE_OPEN_INSTANCE_OBJECT pInstance;

    Status = ObReferenceObjectByHandle(
        hFile,
        0,
        IoFileOpenInstanceObjectType,
        KernelMode,
        &pInstance,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    ASSERT(pInstance != NULL && pInstance->SharedFileObject != NULL);
    *outSection = pInstance->SharedFileObject->hImageSection;

    ObDereferenceObject(pInstance);
    return Status;
}


NTSTATUS
NTAPI
NnxSetImageSection(
    HANDLE hFile,
    HANDLE hSection)
{
    NTSTATUS Status;
    PFILE_OPEN_INSTANCE_OBJECT pInstance;

    Status = ObReferenceObjectByHandle(
        hFile,
        0,
        IoFileOpenInstanceObjectType,
        KernelMode,
        &pInstance,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    ASSERT(pInstance != NULL && pInstance->SharedFileObject != NULL);
    pInstance->SharedFileObject->hImageSection = hSection;

    ObDereferenceObject(pInstance);
    return Status;
}