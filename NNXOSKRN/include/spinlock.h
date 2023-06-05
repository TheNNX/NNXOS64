#ifndef NNX_SPINLOCK_HEADER
#define NNX_SPINLOCK_HEADER

#ifdef __cplusplus
extern "C" {
#endif
#include <nnxtype.h>
#include <irql.h>

#pragma warning(push)
#pragma warning(disable: 4324)
	typedef ULONG_PTR volatile KSPIN_LOCK, * volatile PKSPIN_LOCK;
#pragma warning(pop)

#if defined(NNX_HAL) || defined(NNX_KERNEL)
	NTHALAPI
	VOID
	NTAPI
	KiAcquireSpinLock(
		PKSPIN_LOCK SpinLock);

	NTHALAPI
	VOID
	NTAPI
	KiReleaseSpinLock(
		PKSPIN_LOCK SpinLock);
#endif

#ifndef NNX_HAL
	NTSYSAPI
	KIRQL 
	FASTCALL 
	KfAcquireSpinLock(
		PKSPIN_LOCK lock);

	NTSYSAPI
	VOID 
	FASTCALL 
	KfReleaseSpinLock(
		PKSPIN_LOCK lock,
		KIRQL newIrql);

	NTSYSAPI
	VOID 
	NTAPI 
	KeAcquireSpinLock(
		PKSPIN_LOCK lock,
		PKIRQL oldIrql);

	NTSYSAPI
	VOID 
	NTAPI 
	KeReleaseSpinLock(
		PKSPIN_LOCK lock, 
		KIRQL newIrql);

	NTSYSAPI
	VOID
	NTAPI 
	KeInitializeSpinLock(
		PKSPIN_LOCK lock);

	NTSYSAPI
	VOID
	NTAPI 
	KeAcquireSpinLockAtDpcLevel(
		PKSPIN_LOCK Lock);

	NTSYSAPI
	VOID
	NTAPI 
	KeReleaseSpinLockFromDpcLevel(
		PKSPIN_LOCK Lock);
#else
	inline VOID KeInitializeSpinLock(PKSPIN_LOCK Lock)
	{
		Lock = 0;
	}
#endif

#ifdef __cplusplus
}
#endif

#endif
