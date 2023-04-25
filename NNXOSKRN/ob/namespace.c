#include "object.h"
#include <bugcheck.h>
#include <text.h>
#include <scheduler.h>
#include <cpu.h>

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

HANDLE GlobalNamespace = NULL;
static HANDLE ObpTypeDirHandle = NULL;

static 
NTSTATUS 
DirObjTypeAddChildObject(
    PVOID selfObject, 
    PVOID newObject)
{
    POBJECT_HEADER header, newHeader;
    POBJECT_DIRECTORY selfDir;

    selfDir = (POBJECT_DIRECTORY)selfObject;
    header = ObGetHeaderFromObject(selfObject);
    newHeader = ObGetHeaderFromObject(newObject);
    
    InsertHeadList(
        &selfDir->ChildrenHead, 
        &newHeader->ParentChildListEntry);

    return STATUS_SUCCESS;
}

/* @brief Internal function for opening a child object in a directory object. 
 * All name and path parsing has to be done BEFORE calling this function.
 * @param SelfObject - pointer to the parent directory object
 * @param pOutObject - pointer to a PVOID, where the pointer to the opened 
   object is to be stored
 * @param DesiredAccess
 * @param AccessMode
 * @param KnownName - name (and not the path) of the object
 * @param CaseInsensitive - if true, function ignores case in string 
   comparisons */
static 
NTSTATUS 
DirObjTypeOpenObjectWithNameDecoded(
    PVOID SelfObject, 
    PVOID* pOutObject, 
    ACCESS_MASK DesiredAccess,
    KPROCESSOR_MODE AccessMode,
    PUNICODE_STRING KnownName,
    BOOL CaseInsensitive,
    PVOID OptionalData)
{
    POBJECT_DIRECTORY DirectoryData;
    PLIST_ENTRY CurrentListEntry;

    DirectoryData = (POBJECT_DIRECTORY)SelfObject;
    CurrentListEntry = DirectoryData->ChildrenHead.First;

    /* Iterate over all child items. */
    while (CurrentListEntry != &DirectoryData->ChildrenHead)
    {
        POBJECT_HEADER ObjectHeader;
        ObjectHeader = (POBJECT_HEADER)CurrentListEntry;

        /* If names match. */
        if (RtlEqualUnicodeString(
                &ObjectHeader->Name, 
                KnownName, 
                CaseInsensitive))
        {
            PVOID Object;
            NTSTATUS Status;

            Object = ObGetObjectFromHeader(ObjectHeader);
           
            /* Reference the found object performing access checks. */
            Status = ObReferenceObjectByPointer(
                Object, 
                DesiredAccess, 
                NULL, 
                AccessMode);
            if (Status != STATUS_SUCCESS)
            {
                return Status;
            }

            /* If the object has a custom OnOpen handler, call it. */
            if (ObjectHeader->ObjectType->OnOpen != NULL)
            {
                Status = ObjectHeader->ObjectType->OnOpen(
                    Object, 
                    OptionalData);
                if (Status != STATUS_SUCCESS)
                {
                    ObDereferenceObject(Object);
                    return Status;
                }
            }

            *pOutObject = Object;
            return STATUS_SUCCESS;
        }

        /* Go to next entry. */
        CurrentListEntry = CurrentListEntry->Next;
    }

    return STATUS_OBJECT_NAME_NOT_FOUND;
}

static 
NTSTATUS 
DirObjTypeOpenObject(
    PVOID SelfObject,
    PVOID* pOutObject,
    ACCESS_MASK DesiredAccess,
    KPROCESSOR_MODE AccessMode,
    PUNICODE_STRING Name,
    BOOL CaseInsensitive,
    PVOID OptionalData)
{
    USHORT firstSlashPosition;

    /* If the name of the object is empty, the path is invalid. */
    if (Name == NULL || Name->Length == 0)
    {
        return STATUS_OBJECT_PATH_INVALID;
    }

    /* Find the first slash. */
    for (firstSlashPosition = 0;
         firstSlashPosition < Name->Length / sizeof(*Name->Buffer); 
         firstSlashPosition++)
    {
        if (Name->Buffer[firstSlashPosition] == '\\')
        {
            break;
        }
    }

    if (firstSlashPosition == Name->Length / sizeof(*Name->Buffer))
    {
        /* Last path part. */
        return DirObjTypeOpenObjectWithNameDecoded(
            SelfObject, 
            pOutObject, 
            DesiredAccess, 
            AccessMode, 
            Name, 
            CaseInsensitive,
            OptionalData);
    }
    else if (firstSlashPosition == 0) 
    {
        /* Invalid path (those paths are relative paths). */
        return STATUS_OBJECT_PATH_INVALID;
    }
    else
    {
        /* Parse further. */
        UNICODE_STRING childStr, parentStr;
        POBJECT_HEADER parentHeader;
        NTSTATUS status;
        PVOID nextParent;

        /* Open the first object in the path and try to open the target object
         * recursively in it. */
        childStr.Buffer = Name->Buffer + firstSlashPosition + 1;
        childStr.Length = Name->Length - (firstSlashPosition + 1) 
            * sizeof(*Name->Buffer);
        childStr.MaxLength = Name->Length - (firstSlashPosition + 1) 
            * sizeof(*Name->Buffer);

        parentStr.Buffer = Name->Buffer;
        parentStr.Length = firstSlashPosition * sizeof(*Name->Buffer);
        parentStr.MaxLength = firstSlashPosition * sizeof(*Name->Buffer);

        /* Open the parent directory. */
        status = DirObjTypeOpenObjectWithNameDecoded(
            SelfObject,
            &nextParent,
            DesiredAccess, 
            AccessMode, 
            &parentStr, 
            CaseInsensitive,
            OptionalData);

        if (status != STATUS_SUCCESS)
            return status;

        /* If the found parent object cannot be traversed, 
         * the path is invalid. */
        parentHeader = ObGetHeaderFromObject(nextParent);
        if (parentHeader->ObjectType->ObjectOpen == NULL)
        {
            ObDereferenceObject(nextParent);
            return STATUS_OBJECT_PATH_INVALID;
        }

        /* Recursivly open the shortened child path in parent. */
        status = parentHeader->ObjectType->ObjectOpen(
            nextParent, 
            pOutObject, 
            DesiredAccess, 
            AccessMode,
            &childStr,
            CaseInsensitive,
            OptionalData);
            
        /* Dereference parent and return result of its ObjectOpen. */
        ObDereferenceObject(nextParent);
        return status;
    }

    return STATUS_OBJECT_NAME_NOT_FOUND;
}

static OBJECT_TYPE_IMPL ObTypeTypeImpl = 
{
    {
        {NULL, NULL}, 
        RTL_CONSTANT_STRING(L"Type"), 
        NULL, 
        0,
        0, 
        1, 
        0, 
        {NULL, NULL}, 
        &ObTypeTypeImpl.Data, 
        0
    },
    {
        NULL,
        NULL,
        NULL,
        NULL, 
        NULL, 
        NULL, 
        NULL,
        sizeof(OBJECT_TYPE)
    }
};

static OBJECT_TYPE_IMPL ObDirectoryTypeImpl = 
{
    {
        {NULL, NULL}, 
        RTL_CONSTANT_STRING(L"Directory"), 
        NULL, 
        0, 
        0, 
        1, 
        0, 
        {NULL, NULL}, 
        &ObTypeTypeImpl.Data, 
        0 
    },
    {
        DirObjTypeOpenObject, 
        DirObjTypeAddChildObject, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        NULL,
        sizeof(OBJECT_DIRECTORY)
    }
};

POBJECT_TYPE ObTypeObjectType = &ObTypeTypeImpl.Data;
POBJECT_TYPE ObDirectoryObjectType = &ObDirectoryTypeImpl.Data;

static UNICODE_STRING GlobalNamespaceEmptyName = RTL_CONSTANT_STRING(L"");
static UNICODE_STRING TypesDirName = RTL_CONSTANT_STRING(L"ObjectTypes");

/**
 * @brief This function takes a named object and changes it's root to another one
 * First it checks if the object has any root already. If this is the case, it references
 * the old root handle to retrieve the old root pointer. This old root is then immediately
 * dereferenced (the original reference created for the root-child relation is still
 * in place, though). Then the new root is referenced - if this operation fails, 
 * its status is returned. If the new root accepts the object as its child, the operations
 * is complete. Otherwise, the would-be new root is dereferenced and the original root (if any)
 * is restored. 
 */
NTSTATUS 
NTAPI
ObChangeRoot(
    PVOID object,
    HANDLE newRoot,
    KPROCESSOR_MODE accessMode)
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

    if (rootHandle != NULL)
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

        header->Root = NULL;

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
        if (originalRoot != NULL)
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
    if (originalRoot != NULL)
        ObDereferenceObject(originalRoot);

    return STATUS_SUCCESS;
}

HANDLE 
NTAPI
ObpGetTypeDirHandle()
{
    return ObpTypeDirHandle;
}

NTSTATUS 
NTAPI
ObpInitNamespace()
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttributes, typeDirAttrbiutes;
    POBJECT_DIRECTORY globalNamespaceRoot;
    POBJECT_DIRECTORY typesDirectory;
    HANDLE objectTypeDirHandle;

    /* Initialize root's attributes. */
    InitializeObjectAttributes(
        &objAttributes,
        &GlobalNamespaceEmptyName,
        OBJ_PERMANENT | OBJ_KERNEL_HANDLE,
        NULL,
        NULL);

    /* Create the namespace object. */
    status = ObCreateObject(
        &globalNamespaceRoot, 
        0, 
        KernelMode, 
        &objAttributes,
        ObDirectoryObjectType,
        NULL);

    if (status != STATUS_SUCCESS)
        return status;

    /* Initalize root's children head.s */
    InitializeListHead(&globalNamespaceRoot->ChildrenHead);

    /* If handle creation fails, return. */
    status = ObCreateHandle(
        &GlobalNamespace, 
        KernelMode, 
        (PVOID)globalNamespaceRoot);
    if (status != STATUS_SUCCESS)
        return status;

    /* Initalize ObjectTypes directory attributes. */
    InitializeObjectAttributes(
        &typeDirAttrbiutes,
        &TypesDirName,
        OBJ_PERMANENT | OBJ_KERNEL_HANDLE,
        GlobalNamespace,
        NULL);

    /* Create the ObjectTypes directory object. */
    status = ObCreateObject(
        &typesDirectory,
        0,
        KernelMode,
        &typeDirAttrbiutes,
        ObDirectoryObjectType,
        NULL);

    if (status != STATUS_SUCCESS)
        return status;

    /* Initalize ObjectTypes directory's children head. */
    InitializeListHead(&typesDirectory->ChildrenHead);

    /* Create handle for the ObjectTypes directory. */
    status = ObCreateHandle(
        &objectTypeDirHandle,
        KernelMode,
        (PVOID)typesDirectory);

    if (status != STATUS_SUCCESS)
        return status;

    ObpTypeDirHandle = objectTypeDirHandle;

    /* Add premade type objects to ObjectTypes directory. */
    status = ObChangeRoot(
        (PVOID)ObDirectoryObjectType, 
        objectTypeDirHandle,
        KernelMode);
    if (status != STATUS_SUCCESS)
        return status;

    status = ObChangeRoot(
        (PVOID)ObTypeObjectType, 
        objectTypeDirHandle, 
        KernelMode);
    if (status != STATUS_SUCCESS)
        return status;

    return STATUS_SUCCESS;
}

HANDLE 
NTAPI
ObGetGlobalNamespaceHandle()
{
    return GlobalNamespace;
}
