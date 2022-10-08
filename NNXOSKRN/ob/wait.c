/* this isn't really a object manager part, but i'm to lazy to move it */

#include "object.h"
#include <scheduler.h>
#include <pool.h>
#include <bugcheck.h>
#include <scheduler.h>
#include <HAL/rtc.h>

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
    KIRQL irql;
    PKTHREAD currentThread;
    BOOL done = FALSE;
    ULONG ready = 0;

    /* if there are more objects than the internal wait heads can handle */
    if (Count > THREAD_WAIT_OBJECTS && WaitBlockArray == NULL)
        return STATUS_INVALID_PARAMETER;

    for (i = 0; i < Count; i++)
    {
        DISPATCHER_HEADER* dispHeader = (DISPATCHER_HEADER*)Objects[i];
        KeAcquireSpinLock(&dispHeader->Lock, &irql);
        ready += (dispHeader->SignalState != 0);
        KeReleaseSpinLock(&dispHeader->Lock, irql);
    }

    /* if all objects were ready */
    done |= (WaitType == WaitAll && ready == Count);

    /* if any object was ready */
    done |= (WaitType == WaitAny && ready > 0);

    if (done)
    {
        for (i = 0; i < Count; i++)
        {
            DISPATCHER_HEADER* dispHeader = (DISPATCHER_HEADER*)Objects[i];
            KeAcquireSpinLock(&dispHeader->Lock, &irql);

            if (dispHeader->SignalState && WaitType == WaitAny)
            {
                dispHeader->SignalState--;
                KeReleaseSpinLock(&dispHeader->Lock, irql);
                break;
            }
            /* if Done is true, all objects are signaling */
            else if (WaitType == WaitAll)
            {
                dispHeader->SignalState--;
            }

            KeReleaseSpinLock(&dispHeader->Lock, irql);
        }
    }

    /* no waiting is neccessary */
    if (done)
    {
        return STATUS_SUCCESS;
    }

    currentThread = PspGetCurrentThread();

    /* timeout of lenght 0 and yet, none of the objects were signaling */
    if (pTimeout != NULL && *pTimeout == 0)
    {
        return STATUS_TIMEOUT;
    }
 
    /* lock the current thread */
    KeAcquireSpinLock(&currentThread->ThreadLock, &irql);

    /* if waiting is to be done, IRQL has to be >= DISPATCH_LEVLE*/
    if (irql >= DISPATCH_LEVEL)
        KeBugCheckEx(IRQL_NOT_LESS_OR_EQUAL, (ULONG_PTR)KeWaitForMultipleObjects, irql, 0, 0);

    if (Count <= THREAD_WAIT_OBJECTS)
    {
        WaitBlockArray = currentThread->ThreadWaitBlocks;
    }

    currentThread->NumberOfCurrentWaitBlocks = Count;
    currentThread->NumberOfActiveWaitBlocks = Count - ready;
    currentThread->CurrentWaitBlocks = WaitBlockArray;
    
    /* initializa all wait blocks */
    for (i = 0; i < Count; i++)
    {
        DISPATCHER_HEADER* dispHeader = (DISPATCHER_HEADER*)Objects[i];
        KeAcquireSpinLockAtDpcLevel(&dispHeader->Lock);
        WaitBlockArray[i].Object = (DISPATCHER_HEADER*)Objects[i];
        WaitBlockArray[i].WaitMode = WaitMode;
        WaitBlockArray[i].WaitType = WaitType;
        WaitBlockArray[i].Thread = currentThread;

        /* append the block to the list */
        InsertTailList((PLIST_ENTRY)&dispHeader->WaitHead, &WaitBlockArray[i].WaitEntry);
        KeReleaseSpinLockFromDpcLevel(&dispHeader->Lock);
    }

    /* apply the alertable state and the timeout pointer onto the thread */
    KiHandleObjectWaitTimeout(currentThread, pTimeout, Alertable);

    /* set the threads' state */
    currentThread->ThreadState = THREAD_STATE_WAITING;
    KeReleaseSpinLock(&currentThread->ThreadLock, irql);
    /* manually trigger the scheduler event */
    PspSchedulerNext();

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

VOID
KeUnwaitThread(
    struct _KTHREAD* pThread,
    LONG_PTR WaitStatus,
    LONG PriorityIncrement
)
{
    if (pThread->ThreadLock == 0)
        KeBugCheckEx(SPIN_LOCK_NOT_OWNED, __LINE__, 0, 0, 0);

    if (pThread->ThreadState == THREAD_STATE_WAITING)
    {
        pThread->Timeout = 0;
        pThread->TimeoutIsAbsolute = 0;
        if (pThread->NumberOfCurrentWaitBlocks)
        {
            pThread->NumberOfCurrentWaitBlocks = 0;
            pThread->CurrentWaitBlocks = 0;
        }

        pThread->ThreadState = THREAD_STATE_READY;

        PspInsertIntoSharedQueue(pThread);
    }
}
