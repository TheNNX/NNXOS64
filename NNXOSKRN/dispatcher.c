#include <dispatcher.h>

KSPIN_LOCK DispatcherLock;

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