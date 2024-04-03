#include "handle.h"
#include "object.h"
#include <scheduler.h>
#include <bugcheck.h>
#include <SimpleTextIO.h>

LIST_ENTRY SystemHandleDatabaseHead;
HANDLE_DATABASE InitialSystemHandleDatabase;

KSPIN_LOCK HandleManagerLock;

NTSTATUS 
NTAPI
ObInitHandleManager()
{
    InitializeListHead(&SystemHandleDatabaseHead);
    /* Add one preallocated entry, so we don't have to deal with dynamic memory 
     * (leaks and de)allocation here for now (though we don't really care about
     * memory leaks here, this has to be allocated until the system shutdown 
     * anyway) */
    InsertHeadList(
        &SystemHandleDatabaseHead, 
        &InitialSystemHandleDatabase.HandleDatabaseChainEntry);

    KeInitializeSpinLock(&HandleManagerLock);

    return STATUS_SUCCESS;
}

/* Process->Pcb.ProcessLock has to acquired if process != NULL 
 * @return Process's handle database head or system database head, 
 * if process is NULL. */
PLIST_ENTRY 
NTAPI
ObpGetHandleDatabaseHead(PEPROCESS process)
{
    if (process == NULL)
    {
        return (PLIST_ENTRY)&SystemHandleDatabaseHead;
    }
    return &process->Pcb.HandleDatabaseHead;
}

NTSTATUS 
NTAPI
ObGetHandleDatabaseEntryFromHandle(
    HANDLE handle, 
    PLIST_ENTRY databaseHead,
    PHANDLE_DATABASE_ENTRY* outpEntry)
{
    PLIST_ENTRY current;
    ULONG_PTR currentIndex, handleAsIndex;

    if (handle == NULL)
        return STATUS_INVALID_HANDLE;

    if (outpEntry == NULL)
        return STATUS_INVALID_PARAMETER;

    /* Convert handle to a arithmetic type */
    handleAsIndex = (ULONG_PTR)handle;
    handleAsIndex /= 2;

    currentIndex = 0;

    /* Enumerate the database until we find the database part with the handle */
    current = databaseHead->First;

    while (current)
    {
        /* if the handle is in range of current database part */
        if (currentIndex <= handleAsIndex && 
            currentIndex + ENTRIES_PER_HANDLE_DATABASE > handleAsIndex)
        {
            *outpEntry = 
                &((PHANDLE_DATABASE)current)->
                    Entries[handleAsIndex % ENTRIES_PER_HANDLE_DATABASE];
            return STATUS_SUCCESS;
        }

        currentIndex += ENTRIES_PER_HANDLE_DATABASE;
        current = current->Next;
    }

    return STATUS_INVALID_HANDLE;
}

NTSTATUS 
NTAPI
ObExtractAndReferenceObjectFromHandle(
    HANDLE handle, 
    PVOID* pObject, 
    KPROCESSOR_MODE accessMode)
{
    PHANDLE_DATABASE_ENTRY entry;
    PLIST_ENTRY databaseHead;
    BOOLEAN isKernelHandle;
    PVOID localObject;
    PKSPIN_LOCK lock;
    NTSTATUS status;
    KIRQL irql;

    localObject = NULL;

    if (handle == NULL)
        return STATUS_INVALID_HANDLE;

    /* If the LS bit is set, this is a kernel handle 
     * (and not a handle in current process handle table). */
    isKernelHandle = (((ULONG_PTR)handle) & 0x1) != 0;
    if (isKernelHandle && accessMode == UserMode)
    {
        return STATUS_ACCESS_DENIED;
    }

    /* Lock the database spinlock. */
    if (!isKernelHandle)
    {
        lock = &KeGetCurrentThread()->Process->ProcessLock;
    }
    else
    {
        lock = &HandleManagerLock;
    }
    KeAcquireSpinLock(lock, &irql);

    databaseHead = ObpGetHandleDatabaseHead(
        (isKernelHandle) ?
        NULL :
        CONTAINING_RECORD(KeGetCurrentThread()->Process, EPROCESS, Pcb));

    status = ObGetHandleDatabaseEntryFromHandle(handle, databaseHead, &entry);

    if (status != STATUS_SUCCESS)
    {
        /* Unlock the database spinlock. */
        KeReleaseSpinLock(lock, irql);
        return status;
    }

    /* This is still valid. In theory, it is still referenced, so no clearing,
     * I guess? */
    localObject = entry->Object;

    /* If localObject is still NULL, the object isn't in the entry. */
    if (localObject == NULL)
    {
        /* Unlock the database spinlock. */
        KeReleaseSpinLock(lock, irql);
        return STATUS_INVALID_HANDLE;
    }
    else
    {
        ObReferenceObject(localObject);
    }

    *pObject = localObject;

    /* Unlock the database spinlock. */
    KeReleaseSpinLock(lock, irql);
    return STATUS_SUCCESS;
}

VOID 
NTAPI
ObCloseHandleByEntry(
    PHANDLE_DATABASE_ENTRY entry)
{
    KIRQL irql;
    POBJECT_HEADER objHeader;

    objHeader = ObGetHeaderFromObject(entry->Object);
    KeAcquireSpinLock(&objHeader->Lock, &irql);
    objHeader->HandleCount--;
    KeReleaseSpinLock(&objHeader->Lock, irql);
    ObDereferenceObject(entry->Object);
    entry->Object = NULL;
}

/* @brief Gets the handle database entry for the handle given and performs 
 * ObDestoryHasndleEntry. */
NTSTATUS 
NTAPI
ObCloseHandle(
    HANDLE handle, 
    KPROCESSOR_MODE accessMode)
{
    PHANDLE_DATABASE_ENTRY entry;
    PLIST_ENTRY databaseHead;
    BOOLEAN isKernelHandle;
    PKSPIN_LOCK lock;
    NTSTATUS status;
    KIRQL irql;

    isKernelHandle = (((ULONG_PTR)handle) & 0x01) != 0;

    if (isKernelHandle && accessMode == UserMode)
        return STATUS_ACCESS_DENIED;

    /* Lock the database spinlock. */
    if (!isKernelHandle)
    {
        lock = &KeGetCurrentThread()->Process->ProcessLock;
    }
    else
    {
        lock = &HandleManagerLock;
    }
    KeAcquireSpinLock(lock, &irql);

    /* Get the appropriate handle database. */
    databaseHead = ObpGetHandleDatabaseHead(
        (isKernelHandle) ?
        NULL :
        CONTAINING_RECORD(KeGetCurrentThread()->Process, EPROCESS, Pcb));

    status = ObGetHandleDatabaseEntryFromHandle(handle, databaseHead, &entry);
    if (status != STATUS_SUCCESS)
    {
        /* Unlock the database spinlock. */
        KeReleaseSpinLock(lock, irql);
        return status;
    }
    if (entry->Object == NULL)
    {
        KeReleaseSpinLock(lock, irql);
        return STATUS_INVALID_HANDLE;
    }

    ObCloseHandleByEntry(entry);
    /* Unlock the database spinlock. */
    KeReleaseSpinLock(lock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI
ObCreateHandle(
    PHANDLE pOutHandle, 
    KPROCESSOR_MODE accessMode, 
    PVOID object)
{
    PLIST_ENTRY databaseHead;
    POBJECT_HEADER objHeader;
    PLIST_ENTRY current;
    BOOLEAN isKernelHandle;
    SIZE_T currentIndex;
    KIRQL irql;

    if (pOutHandle == NULL || object == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    objHeader = ObGetHeaderFromObject(object);
    isKernelHandle = (objHeader->Attributes & OBJ_KERNEL_HANDLE) != 0 ||
                      accessMode == KernelMode;

    if (isKernelHandle && accessMode == UserMode)
    {
        return STATUS_ACCESS_DENIED;
    }

    /* Get the appropriate handle database. */
    databaseHead = ObpGetHandleDatabaseHead(
        (isKernelHandle) ?
        NULL :
        CONTAINING_RECORD(KeGetCurrentThread()->Process, EPROCESS, Pcb));

    KeAcquireSpinLock(&HandleManagerLock, &irql);

    currentIndex = 0;
    current = databaseHead->First;

    while (current != databaseHead)
    {
        SIZE_T i;
        PHANDLE_DATABASE handleDatabase;
        handleDatabase = (PHANDLE_DATABASE)current;

        for (i = 0; i < ENTRIES_PER_HANDLE_DATABASE; i++)
        {
            if (handleDatabase->Entries[i].Object == 0)
            {
                *pOutHandle = (HANDLE)((currentIndex * 2) | isKernelHandle);
                handleDatabase->Entries[i].Object = object;
                objHeader->HandleCount++;
                KeReleaseSpinLock(&HandleManagerLock, irql);
                ObReferenceObject(object);
                return STATUS_SUCCESS;
            }

            currentIndex++;
        }

        current = current->Next;
    }

    KeReleaseSpinLock(&HandleManagerLock, irql);
    return STATUS_NO_MEMORY;
}

NTSTATUS
NTAPI
ObCloneHandle(HANDLE InHandle, PHANDLE pOutHandle)
{
    PVOID Object;
    NTSTATUS Status;

    if (pOutHandle == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (InHandle == NULL)
    {
        return STATUS_INVALID_HANDLE;
    }

    Status = ObExtractAndReferenceObjectFromHandle(
        InHandle, 
        &Object, 
        KernelMode);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = ObCreateHandle(pOutHandle, KernelMode, Object);
    ObDereferenceObject(Object);
    return Status;
}