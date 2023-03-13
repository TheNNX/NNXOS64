#include <dispatcher/mutex.h>
#include <scheduler.h>
#include <ntdebug.h>

LONG
NTAPI
KeReleaseMutex(
    PKMUTEX pMutex,
    BOOLEAN Wait)
{
    KIRQL irql;
    PKTHREAD pThread = KeGetCurrentThread();
    ASSERT(pMutex->Owner == pThread);

    KeAcquireSpinLock(&pMutex->Header.Lock, &irql);
    pMutex->Owner = NULL;
    KiSignal(&pMutex->Header, 1);
    if (Wait == FALSE)
    {
        KeReleaseSpinLock(&pMutex->Header.Lock, irql);
    }
    else
    {
        pThread->WaitIrql = irql;
        pThread->WaitIrqlRestore = TRUE;
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
    ASSERT(pMutex->Header.Lock & 1);
    pMutex->Owner = pWaitBlock->Thread;
}