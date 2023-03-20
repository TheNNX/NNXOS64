#include <nnxtype.h>
#include <spinlock.h>
#include <intrin.h>

VOID
NTAPI
KiAcquireSpinLock(
    PKSPIN_LOCK SpinLock)
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
    PKSPIN_LOCK SpinLock)
{

    _InterlockedAnd((PLONG)SpinLock, 0);
    _ReadWriteBarrier();
}