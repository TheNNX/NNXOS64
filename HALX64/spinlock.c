#include <nnxtype.h>
#include <spinlock.h>
#include <intrin.h>

VOID
NTAPI
KiAcquireSpinLock(
    volatile ULONG_PTR* SpinLock)
{
    while (_interlockedbittestandset((PLONG)SpinLock, 0))
    {
        while (*SpinLock & 1)
        {
            _mm_pause();
        }
    }

    _ReadWriteBarrier();
}

VOID
NTAPI
KiReleaseSpinLock(
    volatile ULONG_PTR* SpinLock)
{

    _InterlockedAnd((PLONG)SpinLock, 0);
    _ReadWriteBarrier();
}