#include "ntqueue.h"
#include <scheduler.h>
#include <SimpleTextIO.h>
#include <ntdebug.h>

extern UINT KeNumberOfProcessors;

VOID 
NTAPI
KeInitializeQueue(PKQUEUE Queue, ULONG MaxmimumWaitingThreads) 
{
    InitializeDispatcherHeader(&Queue->Header, QueueObject);

    Queue->MaximumWaitingThreads = 
        (MaxmimumWaitingThreads == 0) ? 
        KeNumberOfProcessors : 
        MaxmimumWaitingThreads;

    Queue->CurrentWaitingThreads = 0;

    InitializeListHead(&Queue->EntryListHead);
    InitializeListHead(&Queue->ThreadsHead);
}

PLIST_ENTRY 
NTAPI
KeRemoveQueue(PKQUEUE Queue, KPROCESSOR_MODE WaitMode, PLONG64 Timeout)
{ 
    NTSTATUS status = STATUS_SUCCESS;

    /* Wait for a signal. */
    status = KeWaitForSingleObject(
        (PVOID)Queue, 
        Executive, 
        WaitMode, 
        FALSE, 
        Timeout);

    if (status == STATUS_SUCCESS)
    {
        return RemoveHeadList(&Queue->EntryListHead);
    }

    return (PLIST_ENTRY)(ULONG_PTR)status;
}

LONG 
NTAPI
KiInsertQueue(PKQUEUE Queue, PLIST_ENTRY Entry, BOOL Head)
{
    LONG initialState;
    KIRQL irql, irql2;

    KeAcquireSpinLock(&Queue->Header.Lock, &irql);
    initialState = Queue->Header.SignalState;

    if (Head)
    {
        InsertHeadList(&Queue->EntryListHead, Entry);
    }
    else
    {
        InsertTailList(&Queue->EntryListHead, Entry);
    }

    irql2 = KiAcquireDispatcherLock();
    KiSignal(&Queue->Header, 1, 0);
    KiReleaseDispatcherLock(irql2);

    KeReleaseSpinLock(&Queue->Header.Lock, irql);
    return initialState;
}

LONG 
NTAPI
KeInsertHeadQueue(PKQUEUE Queue, PLIST_ENTRY Entry)
{
    return KiInsertQueue(Queue, Entry, TRUE);
}

LONG
NTAPI
KeInsertQueue(PKQUEUE Queue, PLIST_ENTRY Entry) 
{ 
    return KiInsertQueue(Queue, Entry, FALSE);
}

VOID
NTAPI
KiUnwaitWaitBlockFromQueue(
    PKWAIT_BLOCK pWaitBlock)
{
    PKQUEUE Queue = (PKQUEUE)pWaitBlock->Object;
    ASSERT(LOCKED(Queue->Header.Lock));

    Queue->CurrentWaitingThreads--;
}