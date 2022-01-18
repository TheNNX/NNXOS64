/* implements NT's KQUEUE's functions */

#include "ntqueue.h"
#include <scheduler.h>
#include <SimpleTextIO.h>

extern UINT KeNumberOfProcessors;

VOID KeInitializeQueue(PKQUEUE Queue, ULONG MaxmimumWaitingThreads) 
{
    KIRQL irql;

    InitializeDispatcherHeader(&Queue->Header, OBJECT_TYPE_KQUEUE);
    KeAcquireSpinLock(&Queue->Header.Lock, &irql);

    Queue->MaximumWaitingThreads = (MaxmimumWaitingThreads == 0) ? KeNumberOfProcessors : MaxmimumWaitingThreads;
    Queue->CurrentWaitingThreads = 0;

    InitializeListHead(&Queue->EntryListHead);
    InitializeListHead(&Queue->ThreadsHead);

    KeReleaseSpinLock(&Queue->Header.Lock, irql);
}

PLIST_ENTRY KeRemvoeQueue(PKQUEUE Queue, KPROCESSOR_MODE WaitMode, PLONG64 Timeout)
{ 
    KIRQL irql;
    PLIST_ENTRY result;
    NTSTATUS status = STATUS_SUCCESS;
    PKTHREAD currentThread;
    PKWAIT_BLOCK waitBlock;

    KeAcquireSpinLock(&Queue->Header.Lock, &irql);

    if (Queue->Header.SignalState)
    {
        Queue->Header.SignalState--;
        result = RemoveHeadList(&Queue->EntryListHead);
        KeReleaseSpinLock(&Queue->Header.Lock, irql);
    }
    else if (Queue->CurrentWaitingThreads < Queue->MaximumWaitingThreads)
    {
        currentThread = KeGetCurrentThread();
        KeAcquireSpinLockAtDpcLevel(&currentThread->ThreadLock);
        Queue->CurrentWaitingThreads++;
        
        /* get one of the waitblocks from the current thread */
        waitBlock = &currentThread->ThreadWaitBlocks[0];
        waitBlock->Object = (PVOID)Queue;
        waitBlock->WaitMode = WaitMode;
        waitBlock->WaitType = WaitAny;
        
        KiHandleObjectWaitTimeout(currentThread, Timeout, FALSE);

        /* add to the thread wait list */
        InsertTailList((PLIST_ENTRY)&currentThread->WaitHead, &waitBlock->WaitEntry);
        currentThread->ThreadState = THREAD_STATE_WAITING;
        KeReleaseSpinLockFromDpcLevel(&currentThread->ThreadLock);
        KeReleaseSpinLock(&Queue->Header.Lock, irql);

        PspSchedulerNext();

        status = currentThread->WaitStatus;

        if (status != STATUS_SUCCESS)
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
    PLIST_ENTRY_POINTER waitEntry, threadsWaitEntry;
    PKWAIT_BLOCK waitBlock;
    PKTHREAD thread;

    KeAcquireSpinLock(&Queue->Header.Lock, &irql);

    initialState = Queue->Header.SignalState;

    if (!IsListEmpty(&Queue->Header.WaitHead))
    {
        waitEntry = (PLIST_ENTRY_POINTER)Queue->Header.WaitHead.First;
        waitBlock = waitEntry->Pointer;

        thread = waitBlock->Thread;
        KeAcquireSpinLockAtDpcLevel(&thread->ThreadLock);

        threadsWaitEntry = FindElementInPointerList(&thread->WaitHead, waitEntry);
        RemoveEntryList((PLIST_ENTRY)threadsWaitEntry);
        Queue->CurrentWaitingThreads--;

        KeReleaseSpinLockFromDpcLevel(&thread->ThreadLock);
        PsCheckThreadIsReady(thread);
    }
    else
    {
        Queue->Header.SignalState++;

        if (Head)
            InsertHeadList(&Queue->EntryListHead, Entry);
        else
            InsertTailList(&Queue->EntryListHead, Entry);
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