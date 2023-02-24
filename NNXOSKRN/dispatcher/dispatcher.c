#include "dispatcher.h"
#include <scheduler.h>
#include "ntqueue.h"
#include <ntdebug.h>

typedef VOID(NTAPI *THREAD_UNWAIT_ROUTINE)(
    PKWAIT_BLOCK pWaitBlock);

typedef VOID(NTAPI *DESIGNAL_ROUTINE)(
    PDISPATCHER_HEADER Self, PKWAIT_BLOCK pWaitBlock);

typedef struct _DISPATCHER_TYPE
{
    THREAD_UNWAIT_ROUTINE pfnOnThreadUnwait;
}DISPATCHER_TYPE, *PDISPATCHER_TYPE;

inline 
VOID 
InitDispatcherType(
    PDISPATCHER_TYPE pDispatcherType,
    THREAD_UNWAIT_ROUTINE ThreadUnwait)
{
    pDispatcherType->pfnOnThreadUnwait = ThreadUnwait;
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
        (THREAD_UNWAIT_ROUTINE)KiUnwaitWaitBlockFromQueue);

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
        NULL);

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
KiUnwaitWaitBlock(
    PKWAIT_BLOCK pWaitBlock,
    BOOLEAN Designal,
    LONG_PTR WaitStatus,
    LONG PriorityIncrement)
{
    PKTHREAD Thread;
    WAIT_TYPE WaitType;
    PDISPATCHER_HEADER Object = pWaitBlock->Object;
    PDISPATCHER_TYPE DispatcherType = &DispatcherDecodeTable[Object->Type];

    ASSERT(DispatcherLock != 0);
    ASSERT(pWaitBlock != NULL);
    ASSERT(pWaitBlock->Object != NULL);
    ASSERT(pWaitBlock->Thread != NULL);
    ASSERT(pWaitBlock->Thread->ThreadLock != 0);
    ASSERT(pWaitBlock->Object->Lock & 1);
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
    if (Thread->ThreadState == THREAD_STATE_WAITING &&
        (WaitType == WaitAny ||
        Thread->NumberOfActiveWaitBlocks == 0))
    {
        KeUnwaitThreadNoLock(Thread, WaitStatus, PriorityIncrement);
    }
}

VOID
NTAPI
KiSignal(
    PDISPATCHER_HEADER Object,
    ULONG SignalIncrement)
{
    if ((Object->Lock & 1) == 0)
    {
        KeBugCheckEx(SPIN_LOCK_NOT_OWNED, __LINE__, 0, 0, 0);
    }

    while (SignalIncrement &&
           !IsListEmpty(&Object->WaitHead))
    {
        PKWAIT_BLOCK WaitBlock;
        PKTHREAD Thread;
        PLIST_ENTRY Entry = RemoveHeadList(&Object->WaitHead);
        
        WaitBlock = CONTAINING_RECORD(Entry, KWAIT_BLOCK, WaitEntry);
        Thread = WaitBlock->Thread;

        KeAcquireSpinLockAtDpcLevel(&Thread->ThreadLock);
        KiUnwaitWaitBlock(WaitBlock, TRUE, 0, 0);
        KeReleaseSpinLockFromDpcLevel(&Thread->ThreadLock);
    }

    Object->SignalState--;
}
