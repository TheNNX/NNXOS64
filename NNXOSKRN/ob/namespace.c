#include "object.h"
#include <text.h>

typedef struct _OBJECT_DIRECTORY_IMPL
{
    LIST_ENTRY ChildrenHead;
}OBJECT_DIRECTORY, *POBJECT_DIRECTORY;


typedef struct _OBJECT_TYPE_IMPL
{
    struct _OBJECT_HEADER Header;
    struct _OBJECT_TYPE Data;
}OBJECT_TYPE_IMPL, *POBJCECT_TYPE_IMPL;

HANDLE GlobalNamespace = INVALID_HANDLE_VALUE;

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

    for (firstSlashPosition = 0; firstSlashPosition < Name->Length / sizeof(*Name->Buffer); firstSlashPosition++)
    {
        if (Name->Buffer[firstSlashPosition] == '\\')
            break;
    }

    if (firstSlashPosition == Name->Length / sizeof(*Name->Buffer))
    {
        /* last path part */
        return DirObjTypeOpenObjectWithNameDecoded(SelfObject, *pOutObject, DesiredAccess, AccessMode, Name, CaseInsensitive);
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
    {{NULL, NULL}, RTL_CONSTANT_STRING(L"ObjectType"), NULL, 0, 0, 1, 0, {NULL, NULL}, &ObTypeTypeImpl.Data, 0},
    {NULL, NULL, NULL, NULL}
};

static OBJECT_TYPE_IMPL ObDirectoryTypeImpl = {
    {NULL, NULL, RTL_CONSTANT_STRING(L"Directory"), NULL, 0, 0, 1, 0, {NULL, NULL}, &ObTypeTypeImpl.Data, 0},
    {DirObjTypeOpenObject, NULL, NULL, NULL}
};

POBJECT_TYPE ObTypeType = &ObTypeTypeImpl.Data;
POBJECT_TYPE ObDirectoryType = &ObDirectoryTypeImpl.Data;

static const UNICODE_STRING GlobalNamespaceEmptyName = RTL_CONSTANT_STRING(L"");

NTSTATUS ObpInitNamespace()
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttributes;
    POBJECT_DIRECTORY globalNamespaceRoot;
    
    /* initialize attributes */
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

    /* TODO: create handle for global namespace */

    return status;
}

HANDLE ObGetGlobalNamespaceHandle()
{
    return GlobalNamespace;
}