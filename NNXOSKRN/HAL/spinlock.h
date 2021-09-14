#ifndef NNX_SPINLOCK_HEADER
#define NNX_SPINLOCK_HEADER

#ifdef __cplusplus
extern "C" {
#endif
#include <nnxtype.h>
#include "irql.h"

#pragma warning(push)
#pragma warning(disable: 4324)
	typedef __declspec(align(64)) struct KSPIN_LOCK
	{
		UINT64 Lock;
	}KSPIN_LOCK, *PKSPIN_LOCK;
#pragma warning(pop)

	KIRQL FASTCALL KfAcquireSpinLock(PKSPIN_LOCK lock);
	VOID FASTCALL KfReleaseSpinLock(PKSPIN_LOCK lock, KIRQL newIrql);
	VOID NTAPI KeAcquireSpinLock(PKSPIN_LOCK lock, PKIRQL oldIrql);
	VOID NTAPI KeReleaseSpinLock(PKSPIN_LOCK lock, KIRQL newIrql);
#ifdef __cplusplus
}
#endif

#endif
