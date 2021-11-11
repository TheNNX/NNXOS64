#include "spinlock.h"

KIRQL FASTCALL KfAcquireSpinLock(PKSPIN_LOCK lock) 
{
	KIRQL temp;
	KeRaiseIrql(DISPATCH_LEVEL, &temp);
	HalAcquireLockRaw(&lock->Lock);
	return temp;
}

VOID FASTCALL KfReleaseSpinLock(PKSPIN_LOCK lock, KIRQL newIrql) 
{
	HalReleaseLockRaw(&lock->Lock);
	KeLowerIrql(newIrql);
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
}