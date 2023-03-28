#include <spinlock.h>
#include <bugcheck.h>

VOID 
NTAPI 
KeAcquireSpinLockAtDpcLevel(
    PKSPIN_LOCK Lock)
{
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
    {
        KeBugCheckEx(
            IRQL_NOT_DISPATCH_LEVEL, 
            __LINE__,
            KeGetCurrentIrql(),
            0,
            0);
    }

    KiAcquireSpinLock(Lock);
}

VOID 
NTAPI 
KeReleaseSpinLockFromDpcLevel(
    PKSPIN_LOCK Lock)
{
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
    {
        KeBugCheckEx(
            IRQL_NOT_DISPATCH_LEVEL, 
            __LINE__,
            KeGetCurrentIrql(),
            0, 
            0);
    }

    KiReleaseSpinLock(Lock);
}

KIRQL 
FASTCALL 
KfAcquireSpinLock(
    PKSPIN_LOCK lock) 
{
	KIRQL temp = 0;
	KeRaiseIrql(DISPATCH_LEVEL, &temp);
    KiAcquireSpinLock(lock);

	return temp;
}

VOID 
FASTCALL 
KfReleaseSpinLock(
    PKSPIN_LOCK lock, 
    KIRQL newIrql) 
{
    if (KeGetCurrentIrql() == PASSIVE_LEVEL)
    {
        KeBugCheckEx(
            IRQL_NOT_GREATER_OR_EQUAL, 
            __LINE__, 
            KeGetCurrentIrql(), 
            0, 
            0);
    }
    KiReleaseSpinLock(lock);
	KeLowerIrql(newIrql);
}

VOID 
NTAPI 
KeAcquireSpinLock(
    PKSPIN_LOCK lock, 
    PKIRQL oldIrql) 
{	
	*oldIrql = KfAcquireSpinLock(lock);
}

VOID 
NTAPI 
KeReleaseSpinLock(
    PKSPIN_LOCK lock, 
    KIRQL newIrql) 
{
	KfReleaseSpinLock(lock, newIrql);
}

VOID 
NTAPI 
KeInitializeSpinLock(
    PKSPIN_LOCK lock)
{
	*lock = 0;
}