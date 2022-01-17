#ifndef NNX_SPINLOCK_HEADER
#define NNX_SPINLOCK_HEADER

#ifdef __cplusplus
extern "C" {
#endif
#include <nnxtype.h>
#include <HAL/irql.h>
#include <SimpleTextIO.h>

#pragma warning(push)
#pragma warning(disable: 4324)
	typedef ULONG_PTR volatile KSPIN_LOCK, * volatile PKSPIN_LOCK;
#pragma warning(pop)

	KIRQL FASTCALL KfAcquireSpinLock(PKSPIN_LOCK lock);
	VOID FASTCALL KfReleaseSpinLock(PKSPIN_LOCK lock, KIRQL newIrql);
	VOID NTAPI KeAcquireSpinLock(PKSPIN_LOCK lock, PKIRQL oldIrql);
	VOID NTAPI KeReleaseSpinLock(PKSPIN_LOCK lock, KIRQL newIrql);
	VOID NTAPI KeInitializeSpinLock(PKSPIN_LOCK lock);
	VOID NTAPI KeAcquireSpinLockAtDpcLevel(PKSPIN_LOCK Lock);
	VOID NTAPI KeReleaseSpinLockFromDpcLevel(PKSPIN_LOCK Lock);

	VOID HalAcquireLockRaw(PKSPIN_LOCK lock);
	VOID HalReleaseLockRaw(PKSPIN_LOCK lock);
#ifdef __cplusplus
}
#endif

#endif
