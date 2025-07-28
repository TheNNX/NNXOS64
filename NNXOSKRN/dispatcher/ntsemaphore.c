#include <ntsemaphore.h>

#include <SimpleTextIO.h>
#include <ntdebug.h>
#include <scheduler.h>

NTSYSAPI 
VOID 
NTAPI 
KeInitializeSemaphore(PKSEMAPHORE Semaphore,
                      LONG Count,
                      LONG Limit)
{
    InitializeDispatcherHeader(&Semaphore->Header, SemaphoreObject);

    Semaphore->Limit = Limit;
    Semaphore->Header.SignalState = Count;
}

NTSYSAPI 
LONG 
NTAPI 
KeReleaseSemaphore(PKSEMAPHORE Semaphore,
                   LONG PriorityIncrement,
                   LONG Adjustment,
                   BOOLEAN Wait)
{
    KIRQL irql, irql2;
    LONG preIncrementValue;

    KeAcquireSpinLock(&Semaphore->Header.Lock, &irql);

    /* Get the previous preIncrementValue */
    preIncrementValue = Semaphore->Header.SignalState;
    if (preIncrementValue + Adjustment > Semaphore->Limit)
    {
        /* TODO: throw SEH (very unimplemented) */
        PrintT("[" __FILE__ ":%d] Semaphore limit exceeded - Unimplemented\n", __LINE__);
        PrintT("Preincrement %X, Adjustment %X, Limit %X\n", preIncrementValue, Adjustment, Semaphore->Limit);
        ASSERT(FALSE);
    }

    irql2 = KiAcquireDispatcherLock();
    KiSignal(&Semaphore->Header, Adjustment, PriorityIncrement);
    KiReleaseDispatcherLock(irql2);

    if (Wait == FALSE)
    {
        KeReleaseSpinLock(&Semaphore->Header.Lock, irql);
    }
    else
    {
        KeGetCurrentThread()->WaitIrql = irql;
        KeGetCurrentThread()->WaitIrqlRestore = TRUE;
        KiReleaseSpinLock(&Semaphore->Header.Lock);
    }

    return preIncrementValue;
}

NTSYSAPI 
LONG 
NTAPI 
KeReadStateSemaphore(PKSEMAPHORE Semaphore)
{
    KIRQL irql;
    LONG value;

    KeAcquireSpinLock(&Semaphore->Header.Lock, &irql);
    value = Semaphore->Header.SignalState;
    KeReleaseSpinLock(&Semaphore->Header.Lock, irql);
    
    return value;
}

VOID 
NTAPI 
KiUnwaitWaitBlockFromSemaphore(PKWAIT_BLOCK pWaitBlock)
{
    PKSEMAPHORE Semaphore = (PKSEMAPHORE)pWaitBlock->Object;
    ASSERT(LOCKED(Semaphore->Header.Lock));

    PrintT("Waitblock unwaiting %X, sem %X(%i)\n", pWaitBlock, Semaphore, Semaphore->Header.SignalState);
}