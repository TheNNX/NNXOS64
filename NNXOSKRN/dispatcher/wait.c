#include "dispatcher.h"
#include <scheduler.h>
#include <pool.h>
#include <bugcheck.h>
#include <scheduler.h>
#include <HAL/rtc.h>
#include <ntdebug.h>

/* this is probably wrong somewhere */
VOID KiHandleObjectWaitTimeout(PKTHREAD Thread, PLONG64 pTimeout, BOOL Alertable)
{
    /* if Timeout != NULL && *Timeout == 0 is handled earlier */
    if (pTimeout == NULL)
    {
        Thread->TimeoutIsAbsolute = TRUE;
        Thread->Timeout = UINT64_MAX;
    }
    else
    {
        Thread->TimeoutIsAbsolute = TRUE;

        if (*pTimeout < 0)
        {
            Thread->TimeoutIsAbsolute = FALSE;
            *pTimeout = -*pTimeout;
        }

        Thread->Timeout = *pTimeout;
    }

    /* for APCs (not implemented yet) */
    Thread->Alertable = Alertable;
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

    KiReleaseDispatcherLock(Irql);
    /* Force a clock tick - if the thread is waiting, it will have the control 
     * back only when the wait conditions are satisfied. */
    KeForceClockTick();
    return STATUS_SUCCESS;
}

NTSTATUS KeWaitForSingleObject(PVOID Object, KWAIT_REASON WaitReason, KPROCESSOR_MODE WaitMode, BOOL Alertable, PLONG64 Timeout)
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

    for (i = 0; i < pThread->NumberOfCurrentWaitBlocks; i++)
    {
        if (pThread->CurrentWaitBlocks[i].Object != NULL)
        {
            KiUnwaitWaitBlock(
                &pThread->CurrentWaitBlocks[i],
                FALSE,
                0,
                0);
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
    KeUnwaitThreadNoLock(pThread, WaitStatus, PriorityIncrement);
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

    pThread->Timeout = 0;
    pThread->TimeoutIsAbsolute = 0;
    if (pThread->NumberOfCurrentWaitBlocks)
    {
        RundownWaitBlocks(pThread);
    }

    pThread->WaitStatus = WaitStatus;
}
