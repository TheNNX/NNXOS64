#include <ntmutex.h>
#include <scheduler.h>
#include <ntdebug.h>

LONG
NTAPI
KeReleaseMutex(
    PKMUTEX pMutex,
    BOOLEAN Wait)
{
    KIRQL irql, irql2;
    PKTHREAD pThread = KeGetCurrentThread();
    ASSERT(pMutex->Owner == pThread);

    KeAcquireSpinLock(&pMutex->Header.Lock, &irql);
    pMutex->Owner = NULL;

    irql2 = KiAcquireDispatcherLock();
    KiSignal(&pMutex->Header, 1, 0);
    KiReleaseDispatcherLock(irql2);

    if (Wait == FALSE)
    {
        KeReleaseSpinLock(&pMutex->Header.Lock, irql);
    }
    else
    {
        pThread->WaitIrql = irql;
        pThread->WaitIrqlRestore = TRUE;
        KiReleaseSpinLock(&pMutex->Header.Lock);
    }
    return 0;
}

VOID
NTAPI
KiUnwaitWaitBlockMutex(
    PKWAIT_BLOCK pWaitBlock)
{
    PKMUTEX pMutex = (PKMUTEX)pWaitBlock->Object;
    ASSERT(pMutex->Header.Type == MutexObject);
    ASSERT(LOCKED(pMutex->Header.Lock));
    pMutex->Owner = pWaitBlock->Thread;
}