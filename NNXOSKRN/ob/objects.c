#include "object.h"
#include <pool.h>

NTSTATUS ObpDeleteObject(PVOID Object);

NTSTATUS ObReferenceObjectByHandle(
    HANDLE handle,
    ACCESS_MASK desiredAccess,
    POBJECT_TYPE objectType,
    KPROCESSOR_MODE accessMode,
    PVOID* pObject,
    PVOID unused
)
{
    if (handle == INVALID_HANDLE_VALUE)
        return STATUS_INVALID_HANDLE;

    if (pObject == NULL)
        return STATUS_INVALID_PARAMETER;


}

NTSTATUS ObReferenceObject(PVOID object)
{
    POBJECT_HEADER header;
    KIRQL irql;

    header = ObGetHeaderFromObject(object);

    KeAcquireSpinLock(&header->Lock, &irql);
    header->ReferenceCount++;
    KeReleaseSpinLock(&header->Lock, irql);

    return STATUS_SUCCESS;
}

NTSTATUS ObReferenceObjectByPointer(
    PVOID object,
    ACCESS_MASK desiredAccess,
    POBJECT_TYPE objectType,
    KPROCESSOR_MODE accessMode
)
{
    POBJECT_HEADER header;
    KIRQL irql;
    
    if (object == NULL)
        return STATUS_INVALID_PARAMETER;
    
    header = ObGetHeaderFromObject(object);

    KeAcquireSpinLock(&header->Lock, &irql);

    /* check the object attributes */
    if (header->Attributes & ~(OBJ_VALID_ATTRIBUTES))
    {
        KeReleaseSpinLock(&header->Lock, irql);
        return STATUS_INVALID_PARAMETER;
    }

    /* if checks should be done */
    if (header->Attributes & OBJ_FORCE_ACCESS_CHECK || accessMode != KernelMode)
    {
        /* if there's an object mismatch (objectType == NULL means that no check is done) */
        if (header->ObjectType != objectType && objectType != NULL)
        {
            KeAcquireSpinLock(&header->Lock, irql);
            return STATUS_OBJECT_TYPE_MISMATCH;
        }
        
        /* if there are desired access bits that are not set in granted access */
        if (desiredAccess & ~header->Access)
        {
            KeAcquireSpinLock(&header->Lock, irql);
            return STATUS_ACCESS_DENIED;
        }
    }

    header->ReferenceCount++;

    KeReleaseSpinLock(&header->Lock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS ObDereferenceObject(PVOID object)
{
    POBJECT_HEADER header;
    KIRQL irql;

    header = ObGetHeaderFromObject(object);

    KeAcquireSpinLock(&header->Lock, &irql);
    header->ReferenceCount--;

    if (header->ReferenceCount == 0)
    {
        NTSTATUS status;

        /* clear the name before releasing lock so it's not accessible by name */
        header->Name.Buffer = 0;
        header->Name.Length = 0;
        header->Name.MaxLength = 0;

        KeReleaseSpinLock(&header->Lock, irql);

        status = ObpDeleteObject(object);
        if (status != STATUS_SUCCESS)
            return status;
    }
    else
    {
        KeReleaseSpinLock(&header->Lock, irql);
    }

    return STATUS_SUCCESS;
}

NTSTATUS ObpDeleteObject(PVOID object)
{
    PHANDLE_DATABASE_ENTRY currentHandleEntry;
    POBJECT_HEADER header, rootHeader;
    PVOID rootObject;
    NTSTATUS status;
    KIRQL irql;

    header = ObGetHeaderFromObject(object);

    /* if has a root, remove own entry from root's children */
    if (header->Root)
    {
        status = ObReferenceObjectByHandle(header->Root, 0, NULL, KernelMode, &rootObject, NULL);
        if (status != STATUS_SUCCESS)
            return status;

        rootHeader = ObGetHeaderFromObject(rootObject);

        KeAcquireSpinLock(&rootHeader->Lock, &irql);
        RemoveEntryList(&header->ParentChildListEntry);
        KeReleaseSpinLock(&rootHeader->Lock, irql);

        /* dereference two times, 
         * one time for ObReferenceObjectByHandle 
         * and one for when root was assigned to the object */
        ObDereferenceObject(rootObject);
        ObDereferenceObject(rootObject);
    }

    /* destroy all handles */
    currentHandleEntry = (PHANDLE_DATABASE_ENTRY)header->HandlesHead.First;
    
    while (currentHandleEntry != (PHANDLE_DATABASE_ENTRY)&header->HandlesHead)
    {
        PHANDLE_DATABASE_ENTRY next;

        next = (PHANDLE_DATABASE_ENTRY)currentHandleEntry->ObjectHandleEntry.Next;
        ObDestroyHandleEntry(currentHandleEntry);
        currentHandleEntry = next;
    }

    ExFreePool(header);

    return STATUS_SUCCESS;
}

NTSTATUS ObCreateObject(
    PVOID* pObject, 
    ACCESS_MASK DesiredAccess, 
    KPROCESSOR_MODE AccessMode, 
    POBJECT_ATTRIBUTES Attributes,
    ULONG ObjectSize,
    POBJECT_TYPE objectType
)
{
    HANDLE root;
    NTSTATUS status;
    PVOID rootObject;
    POBJECT_HEADER header;
    POBJECT_TYPE rootType;
    PVOID potentialCollision;

    if (pObject == NULL)
        return STATUS_INVALID_PARAMETER;

    root = Attributes->Root;
    if (root == INVALID_HANDLE_VALUE)
        root = ObGetGlobalNamespaceHandle();

    /* reference the root handle to get access to root's type */
    status = ObReferenceObjectByHandle(
        root, 
        DesiredAccess, 
        NULL, 
        AccessMode, 
        &rootObject, 
        NULL
    );

    if (status != STATUS_SUCCESS)
        return status;

    /* try traversing root to the object - if it succeded, object with this name already exists */
    status = rootType->Traverse(
        rootObject, 
        &potentialCollision, 
        DesiredAccess, 
        AccessMode, 
        Attributes->ObjectName,
        Attributes->Attributes & OBJ_CASE_INSENSITIVE != 0
    );

    if (status == STATUS_SUCCESS)
    {
        /* if OBJ_OPENIF was specifed to this function, simply return the existing object */
        /* TODO: check if NT checks for types in this situation */
        if (Attributes->Attributes & OBJ_OPENIF)
        {
            *pObject = potentialCollision;
            return STATUS_SUCCESS;
        }
        /* fail otherwise */
        else
        {
            return STATUS_OBJECT_NAME_COLLISION;
        }
    }

    /* allocate header and the object */
    header = ExAllocatePool(NonPagedPool, sizeof(OBJECT_HEADER) + ObjectSize);

    /* if system's out of memory, fail */
    if (header == NULL)
        return STATUS_NO_MEMORY;

    KeInitializeSpinLock(header->Lock);
    header->Access = DesiredAccess;
    header->Attributes = Attributes->Attributes;
    header->Root = root;
    header->ObjectType = objectType;

    /* caller has to somehow close/dereference this object */
    header->ReferenceCount = 1;

    /* initialize the handle list */
    header->HandleCount = 0;
    InitializeListHead(&header->HandlesHead);
    
    /* copy the struct (it doesn't copy the buffer though, but according to MSDN that is a-okay) */
    header->Name = *Attributes->ObjectName;

    *pObject = ObGetObjectFromHeader(header);
    
    /* add to roots children */
    rootType->AddChildObject(root, *pObject);

    return STATUS_SUCCESS;
}
