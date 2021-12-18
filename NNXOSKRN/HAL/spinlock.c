#include "spinlock.h"
#include <HAL/APIC/APIC.h>

KIRQL FASTCALL KfAcquireSpinLock(PKSPIN_LOCK lock) 
{
	KIRQL temp = 0;
	if (ApicNumberOfCoresDetected != ApicNumberOfCoresInitialized)
	{
		lock->LockedDuringInitialization = TRUE;
	}
	else
	{
		KeRaiseIrql(DISPATCH_LEVEL, &temp);
	}
	HalAcquireLockRaw(&lock->Lock);
	return temp;
}

VOID FASTCALL KfReleaseSpinLock(PKSPIN_LOCK lock, KIRQL newIrql) 
{
	HalReleaseLockRaw(&lock->Lock);
	if (lock->LockedDuringInitialization)
	{
		lock->LockedDuringInitialization = FALSE;
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
	lock->Lock = 0ULL;
	lock->LockedDuringInitialization = FALSE;
}