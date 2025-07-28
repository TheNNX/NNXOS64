#include <scheduler.h>
#include <SimpleTextIO.h>
#include <ntdebug.h>

#include <dispatcher.h>
#include <ntmutex.h>
#include <ntqueue.h>
#include <ntsemaphore.h>

typedef VOID(NTAPI *THREAD_UNWAIT_ROUTINE)(
    PKWAIT_BLOCK pWaitBlock);

typedef VOID(NTAPI *DESIGNAL_ROUTINE)(
    PDISPATCHER_HEADER Self, PKWAIT_BLOCK pWaitBlock);

typedef struct _DISPATCHER_TYPE
{
    THREAD_UNWAIT_ROUTINE   pfnOnThreadUnwait;
}DISPATCHER_TYPE, *PDISPATCHER_TYPE;

inline 
VOID 
InitDispatcherType(
    PDISPATCHER_TYPE pDispatcherType,
    THREAD_UNWAIT_ROUTINE ThreadUnwait)
{
    pDispatcherType->pfnOnThreadUnwait  = ThreadUnwait;
}

static DISPATCHER_TYPE DispatcherDecodeTable[32] = { 0 };

KSPIN_LOCK DispatcherLock;
LIST_ENTRY RelativeTimeoutListHead;
LIST_ENTRY AbsoluteTimeoutListHead;

NTSTATUS
NTAPI
KeInitializeDispatcher()
{
    KeInitializeSpinLock(&DispatcherLock);

    InitDispatcherType(
        &DispatcherDecodeTable[QueueObject], 
        KiUnwaitWaitBlockFromQueue);

    InitDispatcherType(
        &DispatcherDecodeTable[ThreadObject],
        NULL);

    InitDispatcherType(
        &DispatcherDecodeTable[ProcessObject],
        NULL);

    InitDispatcherType(
        &DispatcherDecodeTable[EventObject],
        NULL);

    InitDispatcherType(
        &DispatcherDecodeTable[TimerObject],
        NULL);

    InitDispatcherType(
        &DispatcherDecodeTable[MutexObject],
        KiUnwaitWaitBlockMutex);

    InitDispatcherType(
        &DispatcherDecodeTable[SemaphoreObject],
        KiUnwaitWaitBlockFromSemaphore);

    InitializeListHead(&AbsoluteTimeoutListHead);
    InitializeListHead(&RelativeTimeoutListHead);

    return STATUS_SUCCESS;
}

KIRQL 
NTAPI
KiAcquireDispatcherLock()
{
    KIRQL oldIrql = KfRaiseIrql(SYNCH_LEVEL);
    KeAcquireSpinLockAtDpcLevel(&DispatcherLock);
    return oldIrql;
}

VOID 
NTAPI
KiReleaseDispatcherLock(KIRQL oldIrql)
{
    KeReleaseSpinLockFromDpcLevel(&DispatcherLock);
    KeLowerIrql(oldIrql);
}

VOID
NTAPI
KiUnwaitWaitBlock(PKWAIT_BLOCK pWaitBlock,
                  BOOLEAN Designal,
                  LONG_PTR WaitStatus,
                  LONG PriorityIncrement)
{
    PKTHREAD Thread;
    SIZE_T Index;
    WAIT_TYPE WaitType;
    PDISPATCHER_HEADER Object = pWaitBlock->Object;
    PDISPATCHER_TYPE DispatcherType = &DispatcherDecodeTable[Object->Type];

    ASSERT(LOCKED(DispatcherLock));
    ASSERT(pWaitBlock != NULL);
    ASSERT(pWaitBlock->Object != NULL);
    ASSERT(pWaitBlock->Thread != NULL);
    ASSERT(LOCKED(pWaitBlock->Thread->ThreadLock));
    ASSERT(LOCKED(pWaitBlock->Object->Lock));
    Thread = pWaitBlock->Thread;
    WaitType = pWaitBlock->WaitType;
    
    if (Designal)
    {
        Object->SignalState--;
    }

    if (DispatcherType->pfnOnThreadUnwait != NULL)
    {
        DispatcherType->pfnOnThreadUnwait(pWaitBlock);
    }

    RemoveEntryList(&pWaitBlock->WaitEntry);
    pWaitBlock->Object = NULL;
    pWaitBlock->Thread = NULL;
    pWaitBlock->WaitMode = 0;
    pWaitBlock->WaitType = 0;

    Thread->NumberOfActiveWaitBlocks--;

    /* If thread state is not THREAD_STATE_WAITING, it means that this function
     * has already been called from KeUnwaitThread to unwait a leftover 
     * waitblock, calling it again would create a recursive loop. */
    if (Thread->ThreadState == THREAD_STATE_WAITING)
    {
        if (Thread->NumberOfActiveWaitBlocks == 0)
        {
            KeUnwaitThreadNoLock(Thread, WaitStatus, PriorityIncrement);
        }
        else if (WaitType == WaitAny)
        {
            Index = pWaitBlock - Thread->CurrentWaitBlocks;
            KeUnwaitThreadNoLock(Thread, Index, PriorityIncrement);
        }
    }
}

VOID
NTAPI
KiSignal(PDISPATCHER_HEADER Object,
         ULONG SignalIncrement,
         LONG PriorityIncrement)
{
    ASSERT(LOCKED(Object->Lock));
    ASSERT(LOCKED(DispatcherLock));

    Object->SignalState += SignalIncrement;

    while (Object->SignalState > 0 && !IsListEmpty(&Object->WaitHead))
    {
        PKWAIT_BLOCK WaitBlock;
        PKTHREAD Thread;
        PLIST_ENTRY Entry = RemoveHeadList(&Object->WaitHead);
        
        WaitBlock = CONTAINING_RECORD(Entry, KWAIT_BLOCK, WaitEntry);
        Thread = WaitBlock->Thread;

        KeAcquireSpinLockAtDpcLevel(&Thread->ThreadLock);
        KiUnwaitWaitBlock(WaitBlock, TRUE, 0, PriorityIncrement);
        KeReleaseSpinLockFromDpcLevel(&Thread->ThreadLock);
    }
}
