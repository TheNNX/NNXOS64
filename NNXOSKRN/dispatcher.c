#include <dispatcher.h>

KSPIN_LOCK DispatcherLock;

KIRQL KiAcquireDispatcherLock()
{
    KIRQL oldIrql = KfRaiseIrql(SYNCH_LEVEL);
    KeAcquireSpinLockAtDpcLevel(&DispatcherLock);
    return oldIrql;
}

VOID KiReleaseDispatcherLock(KIRQL oldIrql)
{
    KeReleaseSpinLockFromDpcLevel(&DispatcherLock);
    KeLowerIrql(oldIrql);
}