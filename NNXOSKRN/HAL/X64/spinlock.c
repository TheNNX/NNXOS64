#include <HAL/spinlock.h>
#include "APIC.h"
#include <bugcheck.h>

/* kinda hacky, but as long as no locks are acquired after MP initialization but before releasing all initialization locks, it will be fine*/
LONG LockedDuringInitialization = 0;

KSPIN_LOCK PrintLock;

VOID NTAPI KiAcquireSpinLock(PKSPIN_LOCK Lock)
{
    HalAcquireLockRaw(Lock);
}

VOID NTAPI KiReleaseSpinLock(PKSPIN_LOCK Lock)
{
    HalReleaseLockRaw(Lock);
}

VOID NTAPI KeAcquireSpinLockAtDpcLevel(PKSPIN_LOCK Lock)
{
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
        KeBugCheckEx(IRQL_NOT_DISPATCH_LEVEL, __LINE__, KeGetCurrentIrql(), 0, 0);

    KiAcquireSpinLock(Lock);
}

VOID NTAPI KeReleaseSpinLockFromDpcLevel(PKSPIN_LOCK Lock)
{
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
        KeBugCheckEx(IRQL_NOT_DISPATCH_LEVEL, __LINE__, KeGetCurrentIrql(), 0, 0);

    KiReleaseSpinLock(Lock);
}

KIRQL FASTCALL KfAcquireSpinLock(PKSPIN_LOCK lock) 
{
	KIRQL temp = 0;
	KIRQL ignore;

	temp = KeGetCurrentIrql();

	if (KeGetCurrentIrql() > DISPATCH_LEVEL)
		KeLowerIrql(DISPATCH_LEVEL);
	else
		KeRaiseIrql(DISPATCH_LEVEL, &ignore);

    KiAcquireSpinLock(lock);
	return temp;
}

VOID FASTCALL KfReleaseSpinLock(PKSPIN_LOCK lock, KIRQL newIrql) 
{
    KiReleaseSpinLock(lock);

	if (LockedDuringInitialization)
	{
		LockedDuringInitialization--;
	}
	else
	{
		KIRQL ignore;

		if (KeGetCurrentIrql() > newIrql)
			KeLowerIrql(newIrql);
		else
			KeRaiseIrql(newIrql, &ignore);
	}
}

VOID NTAPI KeAcquireSpinLock(PKSPIN_LOCK lock, PKIRQL oldIrql) 
{	
	*oldIrql = KfAcquireSpinLock(lock);
}

VOID NTAPI KeReleaseSpinLock(PKSPIN_LOCK lock, KIRQL newIrql) 
{
	KfReleaseSpinLock(lock, newIrql);
}

VOID NTAPI KeInitializeSpinLock(PKSPIN_LOCK lock)
{
	*lock = 0;
}