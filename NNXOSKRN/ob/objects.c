#include <object.h>
#include <pool.h>
#include <bugcheck.h>
#include <rtl.h>
#include <ntdebug.h>

NTSTATUS 
ObpTestNamespace();

NTSTATUS 
ObpMpTestNamespace();

NTSTATUS 
ObpTest()
{
    NTSTATUS status;

    status = ObpTestNamespace();
    if (status != STATUS_SUCCESS)
        return status;

    return STATUS_SUCCESS;
}

NTSTATUS 
ObpMpTest()
{
    NTSTATUS status;

    status = ObpMpTestNamespace();
    if (status != STATUS_SUCCESS)
        return status;

    return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI
ObInit()
{
    NTSTATUS status;

    PrintT("Starting ObInit\n");

    status = ObInitHandleManager();
    if (status != STATUS_SUCCESS)
        return status;

    PrintT("ObInitHandleManager done\n");

    status = ObpInitNamespace();
    if (status != STATUS_SUCCESS)
        return status;

    PrintT("ObpInitNamespace done\n");

    PrintT("ObInit done\n");

    PrintT("Starting tests\n");
    status = ObpTest();
    PrintT("NTSTATUS=0x%X\n", status);

    return status;
}

NTSTATUS ObpDeleteObject(PVOID Object);

/* @brief This function first tries to reference the object without any checks. 
 * If it succeds, it tries to reference it by pointer. The function dereferences
 * all its references on failure, and leaves one reference on success. */
NTSTATUS 
NTAPI
ObReferenceObjectByHandle(
    HANDLE handle,
    ACCESS_MASK desiredAccess,
    POBJECT_TYPE objectType,
    KPROCESSOR_MODE accessMode,
    PVOID* pObject,
    PVOID unused)
{
    PVOID localObjectPtr;
    NTSTATUS status;

    if (handle == INVALID_HANDLE_VALUE)
        return STATUS_INVALID_HANDLE;

    if (pObject == NULL)
        return STATUS_INVALID_PARAMETER;

    /* Try extracting the object pointer. */
    status = ObExtractAndReferenceObjectFromHandle(
        handle, 
        &localObjectPtr, 
        accessMode);
    if (status != STATUS_SUCCESS)
    {
        return status;
    }

    /* Reference once again, but this time with access checks. This is done to 
     * avoid code duplication, and we open the object first to avoid having it 
     * deleted mid-checking. */
    status = ObReferenceObjectByPointer(
        localObjectPtr, 
        desiredAccess, 
        objectType, 
        accessMode);
    if (status != STATUS_SUCCESS)
    {
        /* Access checks failed. */

        /* Derefence the original refernece. */
        ObDereferenceObject(localObjectPtr);
        return status;
    }

    /* Return our pointer. */
    *pObject = localObjectPtr;

    /* Derefence the original refernece. */
    ObDereferenceObject(localObjectPtr);

    return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI
ObReferenceObject(
    PVOID object)
{
    POBJECT_HEADER header;
    KIRQL irql;

    header = ObGetHeaderFromObject(object);

    /* Acquire the objects lock and increment ref count. */
    KeAcquireSpinLock(&header->Lock, &irql);
    header->ReferenceCount++;
    KeReleaseSpinLock(&header->Lock, irql);

    return STATUS_SUCCESS;
}

/* @brief Does the same thing as ObReferenceObjectByHandle,
 * but only if the access checks succed if objectType == NULL, 
 * objectType check is ignored. */
NTSTATUS 
NTAPI
ObReferenceObjectByPointer(
    PVOID object,
    ACCESS_MASK desiredAccess,
    POBJECT_TYPE objectType,
    KPROCESSOR_MODE accessMode)
{
    POBJECT_HEADER header;
    KIRQL irql;
    
    if (object == NULL)
        return STATUS_INVALID_PARAMETER;

    header = ObGetHeaderFromObject(object);

    KeAcquireSpinLock(&header->Lock, &irql);

    /* If ReferenceCount is 0, the object was deleted when the code was waiting
     * for it's lock. */
    ASSERT(header->ReferenceCount != 0);

    /* Check the object attributes. */
    if (header->Attributes & ~(OBJ_VALID_ATTRIBUTES))
    {
        KeReleaseSpinLock(&header->Lock, irql);
        return STATUS_INVALID_PARAMETER;
    }

    /* Check if the object is a kenrel-only object. */
    if ((header->Attributes & OBJ_KERNEL_HANDLE) && accessMode != KernelMode)
    {
        KeReleaseSpinLock(&header->Lock, irql);
        return STATUS_INVALID_HANDLE;
    }

    /* If checks should be done */
    if (header->Attributes & OBJ_FORCE_ACCESS_CHECK || 
        accessMode != KernelMode)
    {
        /* If there's an object type mismatch 
         * (objectType == NULL means that no check is done) */
        if (header->ObjectType != objectType && objectType != NULL)
        {
            KeReleaseSpinLock(&header->Lock, irql);
            return STATUS_OBJECT_TYPE_MISMATCH;
        }
        
        /* If there are desired access bits 
         * that are not set in granted access */
        if (desiredAccess & ~header->Access)
        {
            KeReleaseSpinLock(&header->Lock, irql);
            return STATUS_ACCESS_DENIED;
        }
    }

    header->ReferenceCount++;
    KeReleaseSpinLock(&header->Lock, irql);

    return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI
ObDereferenceObject(
    PVOID object)
{
    POBJECT_HEADER header;
    KIRQL irql;

    header = ObGetHeaderFromObject(object);

    KeAcquireSpinLock(&header->Lock, &irql);
    header->ReferenceCount--;

    if (header->ReferenceCount == 0)
    {
        NTSTATUS status;
        /* Clear the name before releasing lock,
         * so it's not accessible by name. */
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
    POBJECT_HEADER header, rootHeader;
    PVOID rootObject;
    NTSTATUS status;
    KIRQL irql;

    status = STATUS_SUCCESS;
    header = ObGetHeaderFromObject(object);

    if (header->HandleCount > 0)
    {
        return STATUS_ACCESS_DENIED;
    }

    /* If has a root, remove own entry from root's children. */
    if (header->Root && header->Root != INVALID_HANDLE_VALUE)
    {
        status = ObReferenceObjectByHandle(
            header->Root, 
            0, 
            NULL, 
            KernelMode,
            &rootObject, 
            NULL);
        if (status != STATUS_SUCCESS)
        {
            return status;
        }

        rootHeader = ObGetHeaderFromObject(rootObject);

        KeAcquireSpinLock(&rootHeader->Lock, &irql);
        RemoveEntryList(&header->ParentChildListEntry);
        KeReleaseSpinLock(&rootHeader->Lock, irql);

        /* Dereference two times, one time for ObReferenceObjectByHandle, 
         * and one for when root was assigned to the object. */
        ObDereferenceObject(rootObject);
        ObDereferenceObject(rootObject);
    }

    if (header->ObjectType->OnDelete != NULL)
    {
        status = header->ObjectType->OnDelete(object);
    }

    ExFreePool(header);
    return status;
}

NTSTATUS 
NTAPI
ObCreateObject(
    PVOID* pObject, 
    ACCESS_MASK DesiredAccess, 
    KPROCESSOR_MODE AccessMode, 
    POBJECT_ATTRIBUTES Attributes,
    POBJECT_TYPE objectType,
    PVOID optionalData)
{
    HANDLE root;
    NTSTATUS status;
    PVOID rootObject;
    POBJECT_HEADER header;
    POBJECT_TYPE rootType;
    PVOID potentialCollision;

    if (pObject == NULL)
        return STATUS_INVALID_PARAMETER;

    root = INVALID_HANDLE_VALUE;
    status = STATUS_SUCCESS;
    
    if (Attributes->ObjectName != NULL)
    {
        root = Attributes->Root;
        if (root == INVALID_HANDLE_VALUE)
            root = ObGetGlobalNamespaceHandle();

        /* If root still INVALID_HANDLE_VALUE after setting to global namespace,
         * it means object manager is not initialized yet. */
        if (root != INVALID_HANDLE_VALUE)
        {
            /* Reference the root handle to get access to root's type. */
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

            POBJECT_HEADER rootHeader = ObGetHeaderFromObject(rootObject);
            rootType = rootHeader->ObjectType;

            /* Try traversing root to the object - if it succeded, 
             * object with this name already exists. */
            if (rootType->ObjectOpen == NULL)
            {
                status = STATUS_OBJECT_PATH_INVALID;
            }
            else
            {
                status = rootType->ObjectOpen(
                    rootObject,
                    &potentialCollision,
                    DesiredAccess,
                    AccessMode,
                    Attributes->ObjectName,
                    (Attributes->Attributes & OBJ_CASE_INSENSITIVE) != 0);
            }

            if (status == STATUS_SUCCESS)
            {
                /* If OBJ_OPENIF was specifed to this function, simply 
                 * return the existing object. */
                /* TODO: check if NT checks for types in this situation. */
                if (Attributes->Attributes & OBJ_OPENIF)
                {
                    *pObject = potentialCollision;
                    return STATUS_SUCCESS;
                }
                /* Fail otherwise. */
                else
                {
                    ObDereferenceObject(potentialCollision);
                    return STATUS_OBJECT_NAME_COLLISION;
                }
            }
        }
    }

    /* Allocate header and the object. */
    header = ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(OBJECT_HEADER) + objectType->InstanceSize, 
        'OBJ ');

    /* If system's out of memory, fail. */
    if (header == NULL)
        return STATUS_NO_MEMORY;

    KeInitializeSpinLock(&header->Lock);
    header->Access = DesiredAccess;
    header->Attributes = Attributes->Attributes;
    header->Root = root;
    header->ObjectType = objectType;

    /* Caller has to somehow close/dereference this object. */
    header->ReferenceCount = 1;

    /* Initialize the handle list. */
    header->HandleCount = 0;
    InitializeListHead(&header->HandlesHead);
    
    /* Copy the struct (it doesn't copy the buffer though, 
     * but according to MSDN that is okay). */
    if (Attributes->ObjectName != NULL)
    {
        header->Name = *Attributes->ObjectName;
    }
    else
    {
        header->Name.Buffer = NULL;
        header->Name.MaxLength = 0;
        header->Name.Length = 0;
    }

    *pObject = ObGetObjectFromHeader(header);
    
    /* Add to root's children. */
    if (root != INVALID_HANDLE_VALUE)
    {
        if (rootType->AddChildObject == NULL)
        {
            status = STATUS_INVALID_HANDLE;
        }
        else
        {
            status = rootType->AddChildObject(rootObject, *pObject);
        }
    }

    /* If there was no problem with adding to the root (or no adding was done),
     * and the object type provides a custom OnCreate method, call it. */
    if (status == STATUS_SUCCESS && header->ObjectType->OnCreate != NULL)
    {
        status = header->ObjectType->OnCreate(*pObject, optionalData);
    }

    /* If there was some problem creating the object, delete it. */
    if (status != STATUS_SUCCESS)
    {
        *pObject = NULL;
        /* Derefernce the object to delete it. */
        ObDereferenceObject(pObject);
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
ObCreateType(
    POBJECT_TYPE* pOutObjectType,
    PUNICODE_STRING TypeName,
    SIZE_T InstanceSize)
{
    NTSTATUS Status;
    HANDLE TypeDirHandle;
    POBJECT_TYPE Result;
    OBJECT_ATTRIBUTES TypeObjectAttributes;

    TypeDirHandle = ObpGetTypeDirHandle();
    
    /* Initialize attributes. */
    InitializeObjectAttributes(
        &TypeObjectAttributes,
        TypeName,
        OBJ_KERNEL_HANDLE,
        TypeDirHandle,
        NULL);
    
    Status = ObCreateObject(
        &Result,
        0,
        KernelMode,
        &TypeObjectAttributes,
        ObTypeObjectType,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    RtlZeroMemory(Result, sizeof(OBJECT_TYPE));
    Result->InstanceSize = InstanceSize;
    *pOutObjectType = Result;

    return STATUS_SUCCESS;
}
