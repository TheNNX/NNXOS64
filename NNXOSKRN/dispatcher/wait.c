#include "dispatcher.h"
#include <scheduler.h>
#include <pool.h>
#include <bugcheck.h>
#include <scheduler.h>
#include <HAL/rtc.h>
#include <ntdebug.h>

extern LIST_ENTRY RelativeTimeoutListHead;
extern LIST_ENTRY AbsoluteTimeoutListHead;
extern ULONG_PTR KeMaximumIncrement;

VOID 
NTAPI
KiHandleObjectWaitTimeout(
    PKTHREAD Thread, 
    PLONG64 pTimeout)
{
    if (DispatcherLock == 0)
    {
        KeBugCheckEx(
            SPIN_LOCK_NOT_OWNED, 
            __LINE__, 
            (ULONG_PTR)&DispatcherLock,
            0, 
            0);
    }

    if (Thread->TimeoutEntry.Timeout != 0)
    {
        RemoveEntryList(&Thread->TimeoutEntry.ListEntry);
    }
    Thread->TimeoutEntry.Timeout = 0;
    Thread->TimeoutEntry.TimeoutIsAbsolute = TRUE;

    if (pTimeout != NULL)
    {
        /* Relative is negative */
        if (*pTimeout < 0)
        {
            Thread->TimeoutEntry.TimeoutIsAbsolute = FALSE;
            Thread->TimeoutEntry.Timeout = -*pTimeout;
            InsertTailList(
                &RelativeTimeoutListHead,
                &Thread->TimeoutEntry.ListEntry);
        }
        /* Absolute is positive */
        else
        {
            Thread->TimeoutEntry.TimeoutIsAbsolute = TRUE;
            Thread->TimeoutEntry.Timeout = *pTimeout;
            InsertTailList(
                &AbsoluteTimeoutListHead,
                &Thread->TimeoutEntry.ListEntry);
        }
    }
}

/* TODO: implement timeouts */
NTSTATUS KeWaitForMultipleObjects(
    ULONG Count,
    PVOID *Objects,
    WAIT_TYPE WaitType,
    KWAIT_REASON WaitReason,
    KPROCESSOR_MODE WaitMode,
    BOOLEAN Alertable,
    PLONG64 pTimeout,
    PKWAIT_BLOCK WaitBlockArray
)
{
    ULONG i;
    KIRQL Irql;
    PKWAIT_BLOCK SelectedWaitBlocks;
    PKTHREAD CurrentThread;

    /* if there are more objects than the internal wait heads can handle */
    if (Count > THREAD_WAIT_OBJECTS && WaitBlockArray == NULL)
        return STATUS_INVALID_PARAMETER;
    
    /* Lock all objects and the dispatcher lock. */
    Irql = KiAcquireDispatcherLock();
    CurrentThread = KeGetCurrentThread();

    for (i = 0; i < Count; i++)
    {
        PDISPATCHER_HEADER Header = (PDISPATCHER_HEADER)Objects[i];
        KeAcquireSpinLockAtDpcLevel(&Header->Lock);
    }

    if (WaitBlockArray != NULL)
    {
        SelectedWaitBlocks = WaitBlockArray;
    }
    else
    {
        SelectedWaitBlocks = CurrentThread->ThreadWaitBlocks;
    }

    /* Initialize the wait blocks. */
    for (i = 0; i < Count; i++)
    {
        PDISPATCHER_HEADER Header = (PDISPATCHER_HEADER)Objects[i];
        SelectedWaitBlocks[i].Object = Header;
        SelectedWaitBlocks[i].Thread = CurrentThread;
        SelectedWaitBlocks[i].WaitMode = WaitMode;
        SelectedWaitBlocks[i].WaitType = WaitType;
        InsertTailList(&Header->WaitHead, &SelectedWaitBlocks[i].WaitEntry);
    }

    CurrentThread->NumberOfActiveWaitBlocks = Count;
    CurrentThread->NumberOfCurrentWaitBlocks = Count;
    CurrentThread->CurrentWaitBlocks = SelectedWaitBlocks;
    CurrentThread->ThreadState = THREAD_STATE_WAITING;

    KeAcquireSpinLockAtDpcLevel(&CurrentThread->ThreadLock);
    /* Fullfill all the waits on already signalling objects. */
    for (i = 0; i < Count; i++)
    {
        PDISPATCHER_HEADER Header = SelectedWaitBlocks[i].Object;
        if (Header->SignalState)
        {
            KiUnwaitWaitBlock(&SelectedWaitBlocks[i], TRUE, 0, 0);
        }
    }
    KeReleaseSpinLockFromDpcLevel(&CurrentThread->ThreadLock);

    /* If no objects are still waited for, the thread can stop waiting. */
    if (CurrentThread->NumberOfActiveWaitBlocks == 0 ||
        WaitType == WaitAny)
    {
        CurrentThread->ThreadState = THREAD_STATE_RUNNING;
        CurrentThread->CurrentWaitBlocks = NULL;
    }

    for (i = 0; i < Count; i++)
    {
        PDISPATCHER_HEADER Header = (PDISPATCHER_HEADER)Objects[i];
        KeReleaseSpinLockFromDpcLevel(&Header->Lock);
    }

    CurrentThread->Alertable = Alertable;
    KiHandleObjectWaitTimeout(
        CurrentThread,
        pTimeout);

    if (CurrentThread->NumberOfActiveWaitBlocks > 0 &&
        pTimeout != NULL && *pTimeout == 0)
    {
        KeUnwaitThreadNoLock(CurrentThread, STATUS_TIMEOUT, 0);
        KiReleaseDispatcherLock(Irql);
        return STATUS_TIMEOUT;
    }

    KiReleaseDispatcherLock(Irql);
    if (Irql >= DISPATCH_LEVEL)
    {
        KeBugCheckEx(IRQL_NOT_LESS_OR_EQUAL, 0, Irql, 0, 0);
    }
    /* Force a clock tick - if the thread is waiting, it will have the control 
     * back only when the wait conditions are satisfied. */
    KeForceClockTick();
    return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI
KeWaitForSingleObject(
    PVOID Object, 
    KWAIT_REASON WaitReason, 
    KPROCESSOR_MODE WaitMode, 
    BOOLEAN Alertable, 
    PLONG64 Timeout)
{
    return KeWaitForMultipleObjects(
        1, 
        &Object,
        WaitAll,
        WaitReason,
        WaitMode,
        Alertable, 
        Timeout,
        NULL
    );
}

static
VOID
RundownWaitBlocks(
    PKTHREAD pThread)
{
    ULONG i;

    ASSERT(KeGetCurrentIrql() >= DISPATCH_LEVEL);
    ASSERT(pThread->ThreadLock != 0);
    ASSERT(DispatcherLock != 0);

    for (i = 0; i < pThread->NumberOfCurrentWaitBlocks; i++)
    {
        PDISPATCHER_HEADER Object = pThread->CurrentWaitBlocks[i].Object;

        if (Object != NULL)
        {
            KeAcquireSpinLockAtDpcLevel(
                &Object->Lock);

            KiUnwaitWaitBlock(
                &pThread->CurrentWaitBlocks[i],
                FALSE,
                0,
                0);

            KeReleaseSpinLockFromDpcLevel(
                &Object->Lock);
        }
    }

    ASSERT(pThread->NumberOfActiveWaitBlocks == 0);
    pThread->NumberOfCurrentWaitBlocks = 0;
    pThread->CurrentWaitBlocks = NULL;
}

/**
 * @brief This function cancels any unsatisfied waits (if any).
 * Then, if the thread is not terminated already, it sets changes the thread 
 * state from THREAD_STATE_WAITING to THREAD_STATE_READY.
 */
VOID
NTAPI
KeUnwaitThread(
    PKTHREAD pThread,
    LONG_PTR WaitStatus,
    LONG PriorityIncrement)
{
    KIRQL irql = KiAcquireDispatcherLock();
    KeAcquireSpinLockAtDpcLevel(&pThread->ThreadLock);
    KeUnwaitThreadNoLock(pThread, WaitStatus, PriorityIncrement);
    KeReleaseSpinLockFromDpcLevel(&pThread->ThreadLock);
    KiReleaseDispatcherLock(irql);
}

VOID
NTAPI
KeUnwaitThreadNoLock(
    PKTHREAD pThread,
    LONG_PTR WaitStatus,
    LONG PriorityIncrement)
{
    ASSERT(pThread->ThreadState == THREAD_STATE_WAITING ||
           pThread->ThreadState == THREAD_STATE_TERMINATED);

    if (pThread->ThreadState == THREAD_STATE_WAITING)
    {
        pThread->ThreadState = THREAD_STATE_READY;
        PspInsertIntoSharedQueue(pThread);
    }

    if (pThread->ThreadLock == 0)
        KeBugCheckEx(SPIN_LOCK_NOT_OWNED, __LINE__, 0, 0, 0);

    KiHandleObjectWaitTimeout(pThread, 0);

    if (pThread->NumberOfCurrentWaitBlocks)
    {
        RundownWaitBlocks(pThread);
    }

    pThread->WaitStatus = WaitStatus;
}

static
VOID
KiExpireTimeout(
    PKTIMEOUT_ENTRY pTimeout)
{
    RemoveEntryList(&pTimeout->ListEntry);
    if (pTimeout->OnTimeout != NULL)
    {
        pTimeout->OnTimeout(pTimeout);
    }
}

VOID
NTAPI
KiClockTick()
{
    PLIST_ENTRY Current;
    PKTIMEOUT_ENTRY TimeoutEntry;
    ULONG64 Time = 0;

    if (DispatcherLock == 0)
    {
        KeBugCheckEx(
            SPIN_LOCK_NOT_OWNED,
            __LINE__,
            (ULONG_PTR)&DispatcherLock,
            0,
            0);
    }

    /* FIXME: It seems KeQuerySystemTime has some weird timing bug. 
     * Reading CMOS too often maybe?
     * Maybe trying to read the same value twice fails often 
     * and it gets itself stuck in the loop there? */
#if 1
    KeQuerySystemTime(&Time);

    Current = AbsoluteTimeoutListHead.First;
    while (Current != &AbsoluteTimeoutListHead)
    {
        TimeoutEntry = CONTAINING_RECORD(Current, KTIMEOUT_ENTRY, ListEntry);
        Current = Current->Next;
        
        if (TimeoutEntry->Timeout > Time)
        {
            KiExpireTimeout(TimeoutEntry);
        }
    }
#endif

    Current = RelativeTimeoutListHead.First;
    while (Current != &RelativeTimeoutListHead)
    {
        TimeoutEntry = CONTAINING_RECORD(Current, KTIMEOUT_ENTRY, ListEntry);
        Current = Current->Next;

        /* Temporary solution. */
        if (KeGetCurrentProcessorId() == 0)
        {
            TimeoutEntry->Timeout -= KeMaximumIncrement;
        }
        if ((LONG64)TimeoutEntry->Timeout < 0)
        {
            KiExpireTimeout(TimeoutEntry);
        }
    }
}