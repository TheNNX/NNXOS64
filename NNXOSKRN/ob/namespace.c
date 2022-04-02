#include "object.h"
#include <bugcheck.h>
#include <text.h>

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

    if (Name->Length == 0)
        return STATUS_OBJECT_PATH_INVALID;

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
    {NULL, NULL, NULL, NULL, NULL}
};

static OBJECT_TYPE_IMPL ObDirectoryTypeImpl = {
    {{NULL, NULL}, RTL_CONSTANT_STRING(L"Directory"), INVALID_HANDLE_VALUE, 0, 0, 1, 0, {NULL, NULL}, &ObTypeTypeImpl.Data, 0 },
    {DirObjTypeOpenObject, DirObjTypeAddChildObject, NULL, NULL, NULL}
};

POBJECT_TYPE ObTypeType = &ObTypeTypeImpl.Data;
POBJECT_TYPE ObDirectoryType = &ObDirectoryTypeImpl.Data;

static UNICODE_STRING GlobalNamespaceEmptyName = RTL_CONSTANT_STRING(L"");
static UNICODE_STRING TypesDirName = RTL_CONSTANT_STRING(L"ObjectTypes");

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

    originalRoot = NULL;

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
    ObReferenceObjectByHandle(newRoot, 0, NULL, accessMode, &rootObject, NULL);
    rootHeader = ObGetHeaderFromObject(rootObject);

    KeAcquireSpinLock(&rootHeader->Lock, &irql2);

    rootType = rootHeader->ObjectType; 
    /* try adding object as a child of the new root object - try to undo all prior changes on failure */
    /* fail if there's no rootType->AddChildObject method */
    failed = (rootType->AddChildObject == NULL);

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
        ObDereferenceObject(object);
        ObDereferenceObject(rootObject);

        /* try setting the root back */
        if (originalRoot != NULL)
        {
            status = ObChangeRoot(object, originalRoot, accessMode);
            if (status)
                return statusToReturn;
        }

        return statusToReturn;
    }

    KeReleaseSpinLock(&rootHeader->Lock, irql2);

    KeReleaseSpinLock(&header->Lock, irql1);
    ObDereferenceObject(object);

    /* if the object originally had any parent, dereference the parent */
    if (originalRoot != NULL)
        ObDereferenceObject(originalRoot);

    return STATUS_SUCCESS;
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
        OBJ_PERMANENT,
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
        ObDirectoryType
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
        OBJ_PERMANENT,
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
        ObDirectoryType
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

    /* add premade type objects to ObjectTypes directory */
    status = ObChangeRoot((PVOID)ObDirectoryType, objectTypeDirHandle, KernelMode);
    if (status != STATUS_SUCCESS)
        return status;

    status = ObChangeRoot((PVOID)ObTypeType, objectTypeDirHandle, KernelMode);
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
        ObDirectoryType,
        KernelMode,
        &globalNamespaceObject,
        NULL
    );
    if (status != STATUS_SUCCESS)
        return status;

    /* try opening one of the predefined objects */
    status = ObDirectoryType->ObjectOpen(
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
    status = ObDirectoryType->ObjectOpen(
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
    status = ObDirectoryType->ObjectOpen(
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

NTSTATUS ObpMpTestNamespace()
{
    return STATUS_SUCCESS;
}