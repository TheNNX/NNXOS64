#include "handle.h"
#include "object.h"
#include <scheduler.h>
#include <bugcheck.h>

LIST_ENTRY SystemHandleDatabaseHead;
HANDLE_DATABASE InitialSystemHandleDatabase;

KSPIN_LOCK HandleManagerLock;

NTSTATUS ObInitHandleManager()
{
    InitializeListHead(&SystemHandleDatabaseHead);
    /* add one preallocated entry, so we don't have to deal with dynamic memory (leaks and de)allocation here for now */
    /* (though we don't really care about memory leaks here, this has to be allocated until the system shutdown anyway) */
    InsertHeadList(&SystemHandleDatabaseHead, &InitialSystemHandleDatabase.HandleDatabaseChainEntry);

    KeInitializeSpinLock(&HandleManagerLock);

    return STATUS_SUCCESS;
}

/* process->Pcb.ProcessLock has to acquired if process != NULL*/
/* @return process's handle database head or system database head, if process is NULL */
PLIST_ENTRY ObpGetHandleDatabaseHead(PEPROCESS process)
{
    if (process == NULL)
        return (PLIST_ENTRY) &SystemHandleDatabaseHead;

    if (process->Pcb.ProcessLock == 0)
        KeBugCheckEx(SPIN_LOCK_NOT_OWNED, (ULONG_PTR) & process->Pcb.ProcessLock, __LINE__, 0, 0);

    return &process->Pcb.HandleDatabaseHead;
}

NTSTATUS ObGetHandleDatabaseEntryFromHandle(HANDLE handle, PLIST_ENTRY databaseHead, PHANDLE_DATABASE_ENTRY *outpEntry)
{
    KIRQL irql;
    PLIST_ENTRY current;
    ULONG_PTR currentIndex, handleAsIndex;

    if (handle == INVALID_HANDLE_VALUE)
        return STATUS_INVALID_HANDLE;

    if (outpEntry == NULL)
        return STATUS_INVALID_PARAMETER;

    KeAcquireSpinLock(&HandleManagerLock, &irql);

    /* convert handle to a arithmetic type */
    handleAsIndex = (ULONG_PTR)handle;

    /* skip zero to avoid bugs due to assuming INVALID_HANDLE_VALUE = 0 */
    currentIndex = 1;

    /* enumerate the database until we find the database part with the handle */
    current = databaseHead->First;

    while (current)
    {
        /* if the handle is in range of current database part */
        if (currentIndex <= handleAsIndex && currentIndex + ENTRIES_PER_HANDLE_DATABASE > handleAsIndex)
        {
            *outpEntry = &((PHANDLE_DATABASE)current)->Entries[handleAsIndex % ENTRIES_PER_HANDLE_DATABASE];

            KeReleaseSpinLock(&HandleManagerLock, irql);
            return STATUS_SUCCESS;
        }

        currentIndex += ENTRIES_PER_HANDLE_DATABASE;
        current = current->Next;
    }

    KeReleaseSpinLock(&HandleManagerLock, irql);
    return STATUS_INVALID_HANDLE;
}

NTSTATUS ObExtractAndReferenceObjectFromHandle(HANDLE handle, PVOID *pObject, KPROCESSOR_MODE accessMode)
{
    PHANDLE_DATABASE_ENTRY entry;
    PLIST_ENTRY databaseHead;
    PVOID localObject;
    NTSTATUS status;

    localObject = NULL;

    if (handle == INVALID_HANDLE_VALUE)
        return STATUS_INVALID_HANDLE;

    /* get the appropriate handle database */
    databaseHead = ObpGetHandleDatabaseHead(
        (accessMode == KernelMode) ?
        NULL :
        /* EPROCESS starts with KPROCESS, and KPROCESS never exists alone */
        (PEPROCESS)KeGetCurrentThread()->Process
    );

    status = ObGetHandleDatabaseEntryFromHandle(handle, databaseHead, &entry);
    if (status != STATUS_SUCCESS)
        return status;

    /* This is still valid. In theory, it is still referenced, so no clearing i guess? 
     * (note to self: please, brain, remember to not deallocate memory explicitly from scheduler objects when they're reworked to use this) */
    localObject = entry->Object;

    /* if localObject is still NULL, the object isn't in the entry */
    if (localObject == NULL)
    {
        return STATUS_INVALID_HANDLE;
    }
    else
    {
        ObReferenceObject(localObject);
    }

    *pObject = localObject;

    return STATUS_SUCCESS;
}

VOID ObCloseHandleByEntry(PHANDLE_DATABASE_ENTRY entry)
{
    KIRQL irql;

    KeAcquireSpinLock(&HandleManagerLock, &irql);

    /* remove that from object's handle list */
    RemoveEntryList(&entry->ObjectHandleEntry);
    entry->Object = NULL;

    KeReleaseSpinLock(&HandleManagerLock, irql);
}

/* @brief Gets the handle database entry for the handle given and performs ObDestoryHasndleEntry */
NTSTATUS ObCloseHandle(HANDLE handle, KPROCESSOR_MODE accessMode)
{
    PHANDLE_DATABASE_ENTRY entry;
    PLIST_ENTRY databaseHead;
    NTSTATUS status;

    /* get the appropriate handle database */
    databaseHead = ObpGetHandleDatabaseHead(
        (accessMode == KernelMode) ?
        NULL :
        /* EPROCESS starts with KPROCESS, and KPROCESS never exists alone */
        (PEPROCESS)KeGetCurrentThread()->Process
    );

    status = ObGetHandleDatabaseEntryFromHandle(handle, databaseHead, &entry);
    if (status != STATUS_SUCCESS)
        return status;

    ObCloseHandleByEntry(entry);

    return STATUS_SUCCESS;
}

NTSTATUS ObCreateHandle(PHANDLE* pOutHandle, KPROCESSOR_MODE accessMode, PVOID object)
{
    PLIST_ENTRY databaseHead;
    PLIST_ENTRY current;
    NTSTATUS status;
    KIRQL irql;

    /* get the appropriate handle database */
    databaseHead = ObpGetHandleDatabaseHead(
        (accessMode == KernelMode) ?
        NULL :
        /* EPROCESS starts with KPROCESS, and KPROCESS never exists alone */
        (PEPROCESS)KeGetCurrentThread()->Process
    );

    KeAcquireSpinLock(&HandleManagerLock, &irql);

    current = databaseHead->First;
    while (current != databaseHead)
    {
        PHANDLE_DATABASE_ENTRY handleDatabaseEntry;
        handleDatabaseEntry = (PHANDLE_DATABASE_ENTRY)current;

        if (handleDatabaseEntry->Object == 0)
        {
            handleDatabaseEntry->Object = object;
            KeReleaseSpinLock(&HandleManagerLock, irql);
            return STATUS_SUCCESS;
        }

        current = current->Next;
    }

    KeReleaseSpinLock(&HandleManagerLock, irql);
    return STATUS_NO_MEMORY;
}