#ifndef NNX_SPINLOCK_HEADER
#define NNX_SPINLOCK_HEADER

#ifdef __cplusplus
extern "C" {
#endif
#include <nnxtype.h>
#include <irql.h>
#include <SimpleTextIO.h>

#pragma warning(push)
#pragma warning(disable: 4324)
	typedef ULONG_PTR volatile KSPIN_LOCK, * volatile PKSPIN_LOCK;
#pragma warning(pop)

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

#ifdef NNX_KERNEL
	VOID
	NTAPI
	KiAcquireSpinLock(
		PKSPIN_LOCK SpinLock);

	VOID
	NTAPI
	KiReleaseSpinLock(
		PKSPIN_LOCK SpinLock);
#endif

#ifdef __cplusplus
}
#endif

#endif
