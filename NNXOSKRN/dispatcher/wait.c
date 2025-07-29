#include "dispatcher.h"
#include <scheduler.h>
#include <pool.h>
#include <bugcheck.h>
#include <scheduler.h>
#include <HALX64/include/rtc.h>
#include <ntdebug.h>
#include <SimpleTextIO.h>
#include <time.h>

NTSTATUS 
NTAPI
KeWaitForMultipleObjects(ULONG Count,
                         PVOID *Objects,
                         WAIT_TYPE WaitType,
                         KWAIT_REASON WaitReason,
                         KPROCESSOR_MODE WaitMode,
                         BOOLEAN Alertable,
                         PLONG64 pTimeout,
                         PKWAIT_BLOCK WaitBlockArray)
{
    ULONG i;
    KIRQL Irql;
    PKWAIT_BLOCK SelectedWaitBlocks;
    PKTHREAD CurrentThread;

    /* if there are more objects than the internal wait heads can handle */
    if (Count > THREAD_WAIT_OBJECTS && WaitBlockArray == NULL)
        return STATUS_INVALID_PARAMETER;
    
    /* Lock all objects and the dispatcher lock. */
    Irql = KfRaiseIrql(DISPATCH_LEVEL);
    CurrentThread = KeGetCurrentThread();

    for (i = 0; i < Count; i++)
    {
        PDISPATCHER_HEADER Header = (PDISPATCHER_HEADER)Objects[i];
        KeAcquireSpinLockAtDpcLevel(&Header->Lock);
    }

    KiAcquireDispatcherLock();

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
            KiUnwaitWaitBlock(&SelectedWaitBlocks[i], TRUE, WaitType == WaitAny ? i : 0, 0);
        }
    }
    KeReleaseSpinLockFromDpcLevel(&CurrentThread->ThreadLock);

    for (i = 0; i < Count; i++)
    {
        PDISPATCHER_HEADER Header = (PDISPATCHER_HEADER)Objects[i];
        KeReleaseSpinLockFromDpcLevel(&Header->Lock);
    }

    /* If no objects are still waited for, the thread can stop waiting. */
    if (CurrentThread->NumberOfActiveWaitBlocks == 0 ||
        (WaitType == WaitAny && CurrentThread->NumberOfActiveWaitBlocks < Count))
    {
        KiReleaseDispatcherLock(Irql);
        return STATUS_SUCCESS;
    }
    
    CurrentThread->Alertable = Alertable;
    KiHandleObjectWaitTimeout(
        CurrentThread,
        pTimeout);

    if (CurrentThread->NumberOfActiveWaitBlocks > 0 &&
        pTimeout != NULL && *pTimeout == 0)
    {
        KeUnwaitThreadNoLock(CurrentThread, STATUS_TIMEOUT, 0);
        CurrentThread->ThreadState = THREAD_STATE_RUNNING;
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
    return (NTSTATUS) CurrentThread->WaitStatus;
}

NTSTATUS 
NTAPI
KeWaitForSingleObject(PVOID Object, 
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
        NULL);
}

static
VOID
RundownWaitBlocks(PKTHREAD pThread)
{
    ULONG i;

    ASSERT(KeGetCurrentIrql() >= DISPATCH_LEVEL);
    ASSERT(LOCKED(pThread->ThreadLock));
    ASSERT(LOCKED(DispatcherLock));

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
KeUnwaitThread(PKTHREAD pThread,
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
KeUnwaitThreadNoLock(PKTHREAD pThread,
                     LONG_PTR WaitStatus,
                     LONG PriorityIncrement)
{
    ASSERT(pThread->ThreadState == THREAD_STATE_WAITING ||
           pThread->ThreadState == THREAD_STATE_TERMINATED);

    if (pThread->ThreadState == THREAD_STATE_WAITING)
    {
        if (pThread == KeGetCurrentThread())
        {
            pThread->ThreadState = THREAD_STATE_RUNNING;
        }
        else
        {
            pThread->ThreadState = THREAD_STATE_READY;
            PspInsertIntoSharedQueue(pThread);
            PsNotifyThreadAwaken();
        }
    }

    ASSERT(LOCKED(pThread->ThreadLock));

    KiHandleObjectWaitTimeout(pThread, 0);

    if (pThread->NumberOfCurrentWaitBlocks)
    {
        RundownWaitBlocks(pThread);
    }

    pThread->WaitStatus = WaitStatus;
}
