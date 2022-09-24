#include "object.h"
#include <bugcheck.h>
#include <text.h>
#include <scheduler.h>
#include <HAL/cpu.h>
#include <nnxalloc.h>

#pragma pack(push, 1)

typedef struct _OBJECT_DIRECTORY_IMPL
{
    LIST_ENTRY ChildrenHead;
}OBJECT_DIRECTORY, *POBJECT_DIRECTORY;


typedef struct _OBJECT_TYPE_IMPL
{
    struct _OBJECT_HEADER Header;
    struct _OBJECT_TYPE Data;
}OBJECT_TYPE_IMPL, *POBJCECT_TYPE_IMPL;

#pragma pack(pop)

HANDLE GlobalNamespace = INVALID_HANDLE_VALUE;

static NTSTATUS DirObjTypeAddChildObject(PVOID selfObject, PVOID newObject)
{
    POBJECT_HEADER header, newHeader;
    POBJECT_DIRECTORY selfDir;

    selfDir = (POBJECT_DIRECTORY)selfObject;
    header = ObGetHeaderFromObject(selfObject);
    newHeader = ObGetHeaderFromObject(newObject);
    
    InsertHeadList(&selfDir->ChildrenHead, &newHeader->ParentChildListEntry);

    return STATUS_SUCCESS;
}

/* @brief Internal function for opening a child object in a directory object. 
 * All name and path parsing has to be done BEFORE calling this function.
 * @param SelfObject - pointer to the parent directory object
 * @param pOutObject - pointer to a PVOID, where the pointer to the opened object is to be stored
 * @param DesiredAccess
 * @param AccessMode
 * @param KnownName - name (and not the path) of the object
 * @param CaseInsensitive - if true, function ignores case in string comparisons */
static NTSTATUS DirObjTypeOpenObjectWithNameDecoded(
    PVOID SelfObject, 
    PVOID* pOutObject, 
    ACCESS_MASK DesiredAccess,
    KPROCESSOR_MODE AccessMode,
    PUNICODE_STRING KnownName,
    BOOL CaseInsensitive
)
{
    POBJECT_DIRECTORY directoryData;
    PLIST_ENTRY currentListEntry;

    ObReferenceObject(SelfObject);

    directoryData = (POBJECT_DIRECTORY)SelfObject;
    currentListEntry = directoryData->ChildrenHead.First;

    /* iterate over all child items */
    while (currentListEntry != &directoryData->ChildrenHead)
    {
        POBJECT_HEADER objectHeader;
        objectHeader = (POBJECT_HEADER)currentListEntry;

        /* if names match */
        if (RtlEqualUnicodeString(&objectHeader->Name, KnownName, CaseInsensitive))
        {
            PVOID object;
            NTSTATUS status;

            object = ObGetObjectFromHeader(objectHeader);

            ObDereferenceObject(SelfObject);
           
            /* reference the found object doing access checks */
            status = ObReferenceObjectByPointer(object, DesiredAccess, NULL, AccessMode);
            if (status != STATUS_SUCCESS)
                return status;

            /* if the object has a custom open handler, call it */
            if (objectHeader->ObjectType->OnOpen != NULL)
            {
                status = objectHeader->ObjectType->OnOpen(object);
                /* if the handler failed, return */
                if (status != STATUS_SUCCESS)
                    return status;
            }

            *pOutObject = object;
            return STATUS_SUCCESS;
        }

        /* go to next entry */
        currentListEntry = currentListEntry->Next;
    }

    ObDereferenceObject(SelfObject);
    return STATUS_OBJECT_NAME_NOT_FOUND;
}

static NTSTATUS DirObjTypeOpenObject(
    PVOID SelfObject,
    PVOID* pOutObject,
    ACCESS_MASK DesiredAccess,
    KPROCESSOR_MODE AccessMode,
    PUNICODE_STRING Name,
    BOOL CaseInsensitive
)
{
    USHORT firstSlashPosition;

    /* if the name of the object is empty, the path is invalid */
    if (Name == NULL || Name->Length == 0)
        return STATUS_OBJECT_PATH_INVALID;

    /* find the first slash */
    for (firstSlashPosition = 0; firstSlashPosition < Name->Length / sizeof(*Name->Buffer); firstSlashPosition++)
    {
        if (Name->Buffer[firstSlashPosition] == '\\')
            break;
    }

    if (firstSlashPosition == Name->Length / sizeof(*Name->Buffer))
    {
        /* last path part */
        return DirObjTypeOpenObjectWithNameDecoded(SelfObject, pOutObject, DesiredAccess, AccessMode, Name, CaseInsensitive);
    }
    else if (firstSlashPosition == 0) 
    {
        /* invalid path (those paths are relative paths) */
        return STATUS_OBJECT_PATH_INVALID;
    }
    else
    {
        /* parse further */
        UNICODE_STRING childStr, parentStr;
        POBJECT_HEADER parentHeader;
        NTSTATUS status;
        PVOID parent;

        childStr.Buffer = Name->Buffer + firstSlashPosition + 1;
        childStr.Length = Name->Length - (firstSlashPosition + 1) * sizeof(*Name->Buffer);
        childStr.MaxLength = Name->Length - (firstSlashPosition + 1) * sizeof(*Name->Buffer);

        parentStr.Buffer = Name->Buffer;
        /* index of an element in an array is basically number of elements preceding it */
        parentStr.Length = firstSlashPosition * sizeof(*Name->Buffer);
        parentStr.MaxLength = firstSlashPosition * sizeof(*Name->Buffer);

        /* open the parent directory */
        status = DirObjTypeOpenObjectWithNameDecoded(
            SelfObject,
            &parent,
            DesiredAccess, 
            AccessMode, 
            &parentStr, 
            CaseInsensitive
        );

        if (status != STATUS_SUCCESS)
            return status;

        /* if the found parent object cannot be traversed, the path is invalid */
        parentHeader = ObGetHeaderFromObject(parent);
        if (parentHeader->ObjectType->ObjectOpen == NULL)
        {
            ObDereferenceObject(parent);
            return STATUS_OBJECT_PATH_INVALID;
        }

        /* recursivly open the shortened child path in parent */
        status = parentHeader->ObjectType->ObjectOpen(
            parent, 
            pOutObject, 
            DesiredAccess, 
            AccessMode,
            &childStr,
            CaseInsensitive
        );
            
        /* dereference parent and return result of its ObjectOpen */
        ObDereferenceObject(parent);
        return status;
    }

    return STATUS_OBJECT_NAME_NOT_FOUND;
}

static OBJECT_TYPE_IMPL ObTypeTypeImpl = {
    {{NULL, NULL}, RTL_CONSTANT_STRING(L"Type"), INVALID_HANDLE_VALUE, 0, 0, 1, 0, {NULL, NULL}, &ObTypeTypeImpl.Data, 0},
    {NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

static OBJECT_TYPE_IMPL ObDirectoryTypeImpl = {
    {{NULL, NULL}, RTL_CONSTANT_STRING(L"Directory"), INVALID_HANDLE_VALUE, 0, 0, 1, 0, {NULL, NULL}, &ObTypeTypeImpl.Data, 0 },
    {DirObjTypeOpenObject, DirObjTypeAddChildObject, NULL, NULL, NULL, NULL, NULL}
};

POBJECT_TYPE ObTypeObjectType = &ObTypeTypeImpl.Data;
POBJECT_TYPE ObDirectoryObjectType = &ObDirectoryTypeImpl.Data;

static UNICODE_STRING GlobalNamespaceEmptyName = RTL_CONSTANT_STRING(L"");
static UNICODE_STRING TypesDirName = RTL_CONSTANT_STRING(L"ObjectTypes");

/* @brief This function takes a named object and changes it's root to another one
 * First it checks if the object has any root already. If this is the case, it references
 * the old root handle to retrieve the old root pointer. This old root is then immediately
 * dereferenced (the original reference created for the root-child relation is still
 * in place, though). Then the new root is referenced - if this operation fails, 
 * its status is returned. If the new root accepts the object as its child, the operations
 * is complete. Otherwise, the would-be new root is dereferenced and the original root (if any)
 * is restored. */
NTSTATUS ObChangeRoot(PVOID object, HANDLE newRoot, KPROCESSOR_MODE accessMode)
{
    POBJECT_HEADER header, rootHeader;
    NTSTATUS statusToReturn;
    POBJECT_TYPE rootType;
    PVOID originalRoot;
    KIRQL irql1, irql2;
    HANDLE rootHandle;
    PVOID rootObject;
    NTSTATUS status;
    BOOL failed;

    originalRoot = INVALID_HANDLE_VALUE;

    ObReferenceObject(object);
    header = ObGetHeaderFromObject(object);
    KeAcquireSpinLock(&header->Lock, &irql1);

    rootHandle = header->Root;

    if (rootHandle != INVALID_HANDLE_VALUE)
    {
        /* reference the old root */
        status = ObReferenceObjectByHandle(
            rootHandle,
            0,
            NULL,
            accessMode,
            &rootObject,
            NULL
        );

        if (status != STATUS_SUCCESS)
        {
            KeReleaseSpinLock(&header->Lock, irql1);
            ObDereferenceObject(object);
            return status;
        }

        header->Root = INVALID_HANDLE_VALUE;

        /* remove child entry from child chain in root */
        RemoveEntryList(&header->ParentChildListEntry);

        ObDereferenceObject(rootObject);
        originalRoot = rootObject;
    }

    /* from now on, rootObject is the new root's object */
    /* this is not to be dereferenced, as setting an object as a root increments its reference count */
    status = ObReferenceObjectByHandle(newRoot, 0, NULL, accessMode, &rootObject, NULL);
    if (status != STATUS_SUCCESS)
    {
        KeReleaseSpinLock(&header->Lock, irql1);
        ObDereferenceObject(object);
        return status;
    }

    rootHeader = ObGetHeaderFromObject(rootObject);

    KeAcquireSpinLock(&rootHeader->Lock, &irql2);

    rootType = rootHeader->ObjectType; 
    /* try adding object as a child of the new root object - try to undo all prior changes on failure */
    /* fail if there's no rootType->AddChildObject method */
    failed = (rootType->AddChildObject == NULL);

    statusToReturn = STATUS_SUCCESS;

    /* if it still haven't failed, try calling the method */
    if (failed == FALSE)
    {
        status = rootType->AddChildObject(rootObject, object);
        failed = (status != STATUS_SUCCESS);
    }
    else
    {
        statusToReturn = STATUS_OBJECT_PATH_INVALID;
    }

    /* if it failed, release locks, dereference objects and quit with appriopriate error code */
    if (failed)
    {
        KeReleaseSpinLock(&rootHeader->Lock, irql2);
        KeReleaseSpinLock(&header->Lock, irql1);
        /* try setting the root back */
        if (originalRoot != INVALID_HANDLE_VALUE)
        {
            
            status = ObChangeRoot(object, originalRoot, accessMode);
            if (status)
                statusToReturn = status;
        }
        
        ObDereferenceObject(object);
        ObDereferenceObject(rootObject);

        return statusToReturn;
    }

    header->Root = newRoot;
    KeReleaseSpinLock(&rootHeader->Lock, irql2);

    KeReleaseSpinLock(&header->Lock, irql1);
    ObDereferenceObject(object);

    /* if the object originally had any parent, dereference the parent */
    if (originalRoot != INVALID_HANDLE_VALUE)
        ObDereferenceObject(originalRoot);

    return STATUS_SUCCESS;
}

static HANDLE ObpTypeDirHandle = INVALID_HANDLE_VALUE;

HANDLE ObpGetTypeDirHandle()
{
    return ObpTypeDirHandle;
}

NTSTATUS ObpInitNamespace()
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttributes, typeDirAttrbiutes;
    POBJECT_DIRECTORY globalNamespaceRoot;
    POBJECT_DIRECTORY typesDirectory;
    HANDLE objectTypeDirHandle;

    /* initialize root's attributes */
    InitializeObjectAttributes(
        &objAttributes,
        &GlobalNamespaceEmptyName,
        OBJ_PERMANENT | OBJ_KERNEL_HANDLE,
        NULL,
        NULL
    );

    /* create the namespace object */
    status = ObCreateObject(
        &globalNamespaceRoot, 
        0, 
        KernelMode, 
        &objAttributes, 
        sizeof(OBJECT_DIRECTORY),
        ObDirectoryObjectType,
        NULL
    );

    if (status != STATUS_SUCCESS)
        return status;

    /* initalize root's children head */
    InitializeListHead(&globalNamespaceRoot->ChildrenHead);

    /* if handle creation fails, return */
    status = ObCreateHandle(&GlobalNamespace, KernelMode, (PVOID)globalNamespaceRoot);
    if (status != STATUS_SUCCESS)
        return status;

    /* initalize ObjectTypes directory attributes */
    InitializeObjectAttributes(
        &typeDirAttrbiutes,
        &TypesDirName,
        OBJ_PERMANENT | OBJ_KERNEL_HANDLE,
        GlobalNamespace,
        NULL
    );

    /* create the ObjectTypes directory object */
    status = ObCreateObject(
        &typesDirectory,
        0,
        KernelMode,
        &typeDirAttrbiutes,
        sizeof(OBJECT_DIRECTORY),
        ObDirectoryObjectType,
        NULL
    );

    if (status != STATUS_SUCCESS)
        return status;

    /* initalize ObjectTypes directory's children head */
    InitializeListHead(&typesDirectory->ChildrenHead);

    /* create handle for the ObjectTypes directory */
    status = ObCreateHandle(
        &objectTypeDirHandle,
        KernelMode,
        (PVOID)typesDirectory
    );

    if (status != STATUS_SUCCESS)
        return status;

    ObpTypeDirHandle = objectTypeDirHandle;

    /* add premade type objects to ObjectTypes directory */
    status = ObChangeRoot((PVOID)ObDirectoryObjectType, objectTypeDirHandle, KernelMode);
    if (status != STATUS_SUCCESS)
        return status;

    status = ObChangeRoot((PVOID)ObTypeObjectType, objectTypeDirHandle, KernelMode);
    if (status != STATUS_SUCCESS)
        return status;

    return STATUS_SUCCESS;
}

HANDLE ObGetGlobalNamespaceHandle()
{
    return GlobalNamespace;
}

static UNICODE_STRING ObpTestPath = RTL_CONSTANT_STRING(L"ObjectTypes\\Directory");
static UNICODE_STRING ObpInvalidTestPath = RTL_CONSTANT_STRING(L"ObjectTypes\\");
static UNICODE_STRING ObpNonExistentTestPath = RTL_CONSTANT_STRING(L"ObjectTypes\\Nonexistent");

NTSTATUS ObpTestNamespace()
{
    NTSTATUS status;
    PVOID globalNamespaceObject;
    PVOID directoryTypeObject;
    PVOID invalidObject;

    /* get the pointer to global namespace */
    status = ObReferenceObjectByHandle(
        GlobalNamespace,
        0,
        ObDirectoryObjectType,
        KernelMode,
        &globalNamespaceObject,
        NULL
    );

    if (status != STATUS_SUCCESS)
        return status;

    /* try opening one of the predefined objects */
    status = ObDirectoryObjectType->ObjectOpen(
        globalNamespaceObject,
        &directoryTypeObject,
        0,
        KernelMode,
        &ObpTestPath,
        TRUE
    );

    if (status != STATUS_SUCCESS)
        return status;
    

    /* try opening an invalid object */
    status = ObDirectoryObjectType->ObjectOpen(
        globalNamespaceObject,
        &invalidObject,
        0,
        KernelMode,
        &ObpInvalidTestPath,
        TRUE
    );

    if (status != STATUS_OBJECT_PATH_INVALID)
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, __LINE__, status, STATUS_OBJECT_PATH_INVALID, 0);
    

    /* try opening a non-existent object */
    status = ObDirectoryObjectType->ObjectOpen(
        globalNamespaceObject,
        &invalidObject,
        0,
        KernelMode,
        &ObpNonExistentTestPath,
        TRUE
    );

    if (status != STATUS_OBJECT_NAME_NOT_FOUND)
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, __LINE__, status, STATUS_OBJECT_NAME_NOT_FOUND, 0);

    /* this too, theoreticaly, can fail (on deletion, but we don't expect deletion anyway) */
    status = ObDereferenceObject(globalNamespaceObject);
    if (status != STATUS_SUCCESS)
        return status;

    return STATUS_SUCCESS;
}

NTSTATUS PspCreateProcessInternal(PEPROCESS* ppProcess);
NTSTATUS PspCreateThreadInternal(
    PETHREAD* ppThread,
    PEPROCESS pParentProcess,
    BOOL IsKernel,
    ULONG_PTR EntryPoint
);

#define WORKER_PROCESSES_NUMBER 15
#define WORKER_THREADS_PER_PROCESS 15

UINT NextTesterId = 0;
KSPIN_LOCK NextTesterIdLock;

NTSTATUS WorkerStatus[WORKER_THREADS_PER_PROCESS * WORKER_PROCESSES_NUMBER];
PETHREAD WorkerThreads[WORKER_THREADS_PER_PROCESS * WORKER_PROCESSES_NUMBER];
KWAIT_BLOCK WaitBlocks[WORKER_PROCESSES_NUMBER * WORKER_THREADS_PER_PROCESS];
PEPROCESS WorkerProcesses[WORKER_PROCESSES_NUMBER];

#include <HAL/X64/IDT.h>
#include <HAL/pcr.h>

static NTSTATUS Test1(UINT ownId)
{
    INT i;
    NTSTATUS status;

    for (i = 0; i < 64; i++)
    {
        status = ObpTestNamespace();
        if (status != STATUS_SUCCESS)
        {
            return status;
        }
    }

    return STATUS_SUCCESS;
}

static NTSTATUS Test2()
{
    INT i;
    NTSTATUS status;

    for (i = 0; i < 64; i++)
    {
        PVOID object;
        OBJECT_ATTRIBUTES objAttribs;
        InitializeObjectAttributes(&objAttribs, NULL, OBJ_KERNEL_HANDLE, INVALID_HANDLE_VALUE, 0);

        status = ObCreateObject(
            &object, 
            0, 
            UserMode, 
            &objAttribs,
            sizeof(OBJECT_TYPE),
            ObTypeObjectType,
            NULL
        );

        if (status != STATUS_SUCCESS)
            return status;

        status = ObDereferenceObject(object);
        if (status != STATUS_SUCCESS)
            return status;
    }

    return STATUS_SUCCESS;
}

static VOID ObpWorkerThreadFunction()
{
    UINT ownId;
    KIRQL irql;
    NTSTATUS status;

    /* acquire lock for getting the worker id */
    KeAcquireSpinLock(&NextTesterIdLock, &irql);
    ownId = NextTesterId++;
    KeReleaseSpinLock(&NextTesterIdLock, irql);

    status = Test1(ownId);
    if (status)
        PsExitThread((DWORD)status);
    
    status = Test2();
    if (status)
        PsExitThread((DWORD)status);
        
    PsExitThread(0);
}

VOID DiagnosticThread();

/* @brief this function should be run in a thread */
NTSTATUS ObpMpTestNamespaceThread()
{
    INT i, j;
    NTSTATUS waitStatus;
    UINT64 __lastMemory, __currentMemory;
    char* __caller;
    KIRQL irql;

    KeInitializeSpinLock(&NextTesterIdLock);
    
    /* initialize worker threads with some invalid status values */
    for (i = 0; i < WORKER_THREADS_PER_PROCESS * WORKER_PROCESSES_NUMBER; i++)
        WorkerStatus[i] = (NTSTATUS)(-1);

    SaveStateOfMemory(__FUNCTION__"\n");

    for (i = 0; i < WORKER_PROCESSES_NUMBER; i++)
    {
        KeAcquireSpinLock(&NextTesterIdLock, &irql);
        
        PspCreateProcessInternal(&WorkerProcesses[i]);

        for (j = 0; j < WORKER_THREADS_PER_PROCESS; j++)
        {
            PrintT("[%i/%i] started\n", j + i * WORKER_THREADS_PER_PROCESS + 1, WORKER_PROCESSES_NUMBER * WORKER_THREADS_PER_PROCESS);
            
            PspCreateThreadInternal(
                WorkerThreads + j + i * WORKER_THREADS_PER_PROCESS,
                WorkerProcesses[i],
                TRUE,
                (ULONG_PTR) ObpWorkerThreadFunction
            );

            WorkerThreads[j + i * WORKER_THREADS_PER_PROCESS]->Tcb.ThreadPriority = 1;
            ObReferenceObject(WorkerThreads[j + i * WORKER_THREADS_PER_PROCESS]);

            PspInsertIntoSharedQueue(&WorkerThreads[j + i * WORKER_THREADS_PER_PROCESS]->Tcb);
        }
        KeReleaseSpinLock(&NextTesterIdLock, irql);
    }

    /* wait for all the threads */
    waitStatus = KeWaitForMultipleObjects(
        WORKER_PROCESSES_NUMBER * WORKER_THREADS_PER_PROCESS,
        WorkerThreads,
        WaitAll,
        Executive,
        KernelMode,
        FALSE,
        NULL,
        WaitBlocks
    );


    if (waitStatus != STATUS_SUCCESS)
        PsExitThread(waitStatus);


    for (i = 0; i < WORKER_PROCESSES_NUMBER * WORKER_THREADS_PER_PROCESS; i++)
    {
        if (WorkerThreads[i]->Tcb.ThreadExitCode != STATUS_SUCCESS)
            PsExitThread(WorkerThreads[i]->Tcb.ThreadExitCode);

        ObDereferenceObject(WorkerThreads[i]);
    }

    for (i = 0; i < WORKER_PROCESSES_NUMBER; i++)
    {
        ObDereferenceObject(WorkerProcesses[i]);
    }

    CheckMemory();
    PrintT("Success\n");
    /* deallocation on return from those all things */
    PsExitThread(0);
    return STATUS_SUCCESS;
}

NTSTATUS ObpMpTestNamespace()
{
    PEPROCESS process;
    PETHREAD testThread;
    NTSTATUS status;
    
    if (KeGetCurrentProcessorId() != 0)
        return STATUS_SUCCESS;

    PrintT("%s %i\n", __FUNCTION__, KeGetCurrentProcessorId());

    status = PspCreateProcessInternal(&process);
    if (status != STATUS_SUCCESS)
    {
        PrintT("NTSTATUS: %X\n", status);
        return status;
    }

    status = PspCreateThreadInternal(&testThread, process, TRUE, (ULONG_PTR)ObpMpTestNamespaceThread);
    if (status != STATUS_SUCCESS)
    {
        PrintT("NTSTATUS: %X\n", status);
        return status;
    }
    PrintT("Created namespace test thread %X\n", testThread);

    testThread->Process->Pcb.BasePriority = 10;
    testThread->Tcb.ThreadPriority = 5;
    PspInsertIntoSharedQueue(&testThread->Tcb);

    return STATUS_SUCCESS;
}
