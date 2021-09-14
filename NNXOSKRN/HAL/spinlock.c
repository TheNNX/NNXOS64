#include "spinlock.h"

VOID KiAcquireLockX64(UINT64* lock);
VOID KiReleaseLockX64(UINT64* lock);

KIRQL FASTCALL KfAcquireSpinLock(PKSPIN_LOCK lock) 
{
	KIRQL temp;
	KeRaiseIrql(DISPATCH_LEVEL, &temp);
	KiAcquireLockX64(&lock->Lock);
	return temp;
}

VOID FASTCALL KfReleaseSpinLock(PKSPIN_LOCK lock, KIRQL newIrql) 
{
	KiReleaseLockX64(&lock->Lock);
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