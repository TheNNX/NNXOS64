#include <HAL/spinlock.h>
#include "APIC.h"

/* kinda hacky, but as long as no locks are acquired after MP initialization but before releasing all initialization locks, it will be fine*/
LONG LockedDuringInitialization = 0;

KSPIN_LOCK PrintLock;

VOID NTAPI KeAcquireSpinLockAtDpcLevel(PKSPIN_LOCK Lock)
{
	HalAcquireLockRaw(Lock);
}

VOID NTAPI KeReleaseSpinLockFromDpcLevel(PKSPIN_LOCK Lock)
{
	HalReleaseLockRaw(Lock);
}

KIRQL FASTCALL KfAcquireSpinLock(PKSPIN_LOCK lock) 
{
	KIRQL temp = 0;
	if (ApicNumberOfCoresDetected != ApicNumberOfCoresInitialized)
	{
		LockedDuringInitialization++;
	}
	else
	{
		KeRaiseIrql(DISPATCH_LEVEL, &temp);
	}

	KeAcquireSpinLockAtDpcLevel(lock);
	return temp;
}

VOID FASTCALL KfReleaseSpinLock(PKSPIN_LOCK lock, KIRQL newIrql) 
{
	KeReleaseSpinLockFromDpcLevel(lock);

	if (LockedDuringInitialization)
	{
		LockedDuringInitialization--;
	}
	else
	{
		KeLowerIrql(newIrql);
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