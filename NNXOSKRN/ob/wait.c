#include "object.h"
#include <scheduler.h>
#include <pool.h>

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

    Thread->Alertable = Alertable;
}

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
    BOOL done = FALSE;
    ULONG ready = 0;
    KIRQL originalIrql;

    /* if there are more objects than the internal wait heads can handle */
    if (Count > THREAD_WAIT_OBJECTS && WaitBlockArray == NULL)
        return STATUS_INVALID_PARAMETER;

    /* lock all objects */
    KeRaiseIrql(DISPATCH_LEVEL, &originalIrql);
    for (i = 0; i < Count; i++)
    {
        DISPATCHER_HEADER* dispHeader = (DISPATCHER_HEADER*)Objects[i];
        KeAcquireSpinLockAtDpcLevel(&dispHeader->Lock);
    }

    for (i = 0; i < Count; i++)
    {
        DISPATCHER_HEADER* dispHeader = (DISPATCHER_HEADER*)Objects[i];
        ready += (dispHeader->SignalState != 0);
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

            if (dispHeader->SignalState && WaitType == WaitAny)
            {
                dispHeader->SignalState--;
                break;
            }
            /* if Done is true, all objects are signaling */
            else if (WaitType == WaitAll)
            {
                dispHeader->SignalState--;
            }
        }
    }

    /* release all locks */
    for (i = 0; i < Count; i++)
    {
        DISPATCHER_HEADER* dispHeader = (DISPATCHER_HEADER*)Objects[i];
        KeReleaseSpinLockFromDpcLevel(&dispHeader->Lock);
    }

    /* waiting is neccessary */
    if (!done)
    {
        PKTHREAD currentThread = PspGetCurrentThread();
        ULONG i;

        /* timeout of lenght 0 and yet, none of the objects were signaling */
        if (pTimeout != NULL && *pTimeout == 0)
        {
            KeLowerIrql(originalIrql);
            return STATUS_TIMEOUT;
        }
 
        /* this will elevate us to IRQL = DISPATCH_LEVEL */
        KeAcquireSpinLockAtDpcLevel(&currentThread->ThreadLock);
        
        if (Count > THREAD_WAIT_OBJECTS)
        {
            currentThread->NumberOfCustomThreadWaitBlocks = Count;
            currentThread->CustomThreadWaitBlocks = WaitBlockArray;
        }
        else
        {
            WaitBlockArray = currentThread->ThreadWaitBlocks;
        }

        InitializeListHead((PLIST_ENTRY)&currentThread->WaitHead);
        for (i = 0; i < Count; i++)
        {
            WaitBlockArray[i].Object = (DISPATCHER_HEADER*)Objects[i];
            WaitBlockArray[i].WaitMode = WaitMode;
            WaitBlockArray[i].WaitType = WaitType;
            InsertTailList((PLIST_ENTRY)&currentThread->WaitHead, &WaitBlockArray[i].WaitEntry);
        }

        KiHandleObjectWaitTimeout(currentThread, pTimeout, Alertable);

        KeReleaseSpinLock(&currentThread->ThreadLock, originalIrql);
        PspSchedulerNext();
    }
    else
    {
        KeLowerIrql(originalIrql);
    }

    return STATUS_SUCCESS;
}

NTSTATUS KeWaitForSingleObject(PVOID Object, KWAIT_REASON WaitReason, KPROCESSOR_MODE WaitMode, BOOL Alertable, PLONG64 Timeout)
{
    return KeWaitForMultipleObjects(1, &Object, WaitAll, WaitReason, WaitMode, Alertable, Timeout, NULL);
}