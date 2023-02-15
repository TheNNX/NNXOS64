/* implements NT's KQUEUE's functions */
/* TODO: waiting threads list */

#include "ntqueue.h"
#include <scheduler.h>
#include <SimpleTextIO.h>

extern UINT KeNumberOfProcessors;

VOID KeInitializeQueue(PKQUEUE Queue, ULONG MaxmimumWaitingThreads) 
{
    InitializeDispatcherHeader(&Queue->Header, OBJECT_TYPE_KQUEUE);
    KeAcquireSpinLockAtDpcLevel(&Queue->Header.Lock);

    Queue->MaximumWaitingThreads = (MaxmimumWaitingThreads == 0) ? KeNumberOfProcessors : MaxmimumWaitingThreads;
    Queue->CurrentWaitingThreads = 0;

    InitializeListHead(&Queue->EntryListHead);
    InitializeListHead(&Queue->ThreadsHead);

    KeReleaseSpinLockFromDpcLevel(&Queue->Header.Lock);
}

PLIST_ENTRY KeRemoveQueue(PKQUEUE Queue, KPROCESSOR_MODE WaitMode, PLONG64 Timeout)
{ 
    KIRQL irql;
    PLIST_ENTRY result;
    NTSTATUS status = STATUS_SUCCESS;

    KeAcquireSpinLock(&Queue->Header.Lock, &irql);

    if (Queue->Header.SignalState)
    {
        Queue->Header.SignalState--;
        result = RemoveHeadList(&Queue->EntryListHead);
        KeReleaseSpinLock(&Queue->Header.Lock, irql);
    }
    else if (Queue->CurrentWaitingThreads < Queue->MaximumWaitingThreads)
    {
        KeReleaseSpinLock(&Queue->Header.Lock, irql);
        status = KeWaitForSingleObject((PVOID)Queue, Executive, WaitMode, FALSE, Timeout);

        if (status == STATUS_USER_APC)
            return (PLIST_ENTRY)(ULONG_PTR)status;
        
        KeAcquireSpinLock(&Queue->Header.Lock, &irql);
        result = RemoveHeadList(&Queue->EntryListHead);
        KeReleaseSpinLock(&Queue->Header.Lock, irql);
    }
    else
    {
        KeReleaseSpinLock(&Queue->Header.Lock, irql);
        return (PLIST_ENTRY)(ULONG_PTR)STATUS_TIMEOUT;
    }

    return (status == STATUS_SUCCESS) ? result : (PLIST_ENTRY)(ULONG_PTR)status;
}

LONG KiInsertQueue(PKQUEUE Queue, PLIST_ENTRY Entry, BOOL Head)
{
    LONG initialState;
    KIRQL irql;
    PLIST_ENTRY waitEntry;
    PKWAIT_BLOCK waitBlock;
    PKTHREAD thread;

    KeAcquireSpinLock(&Queue->Header.Lock, &irql);

    initialState = Queue->Header.SignalState;

    if (!IsListEmpty(&Queue->Header.WaitHead))
    {
        waitEntry = (PLIST_ENTRY)Queue->Header.WaitHead.First;
        waitBlock = (PKWAIT_BLOCK)waitEntry;

        thread = waitBlock->Thread;
        Queue->CurrentWaitingThreads--;
        PsCheckThreadIsReady(thread);
    }
    else
    {
        Queue->Header.SignalState++;

        if (Head)
        {
            InsertHeadList(&Queue->EntryListHead, Entry);
        }
        else
        {
            InsertTailList(&Queue->EntryListHead, Entry);
        }
    }

    KeReleaseSpinLock(&Queue->Header.Lock, irql);
    return initialState;
}

LONG KeInsertHeadQueue(PKQUEUE Queue, PLIST_ENTRY Entry)
{
    return KiInsertQueue(Queue, Entry, TRUE);
}

LONG KeInsertQueue(PKQUEUE Queue, PLIST_ENTRY Entry) 
{ 
    return KiInsertQueue(Queue, Entry, FALSE);
}

/* returns first entry if not empty */
PLIST_ENTRY KeRundownQueue(PKQUEUE Queue);