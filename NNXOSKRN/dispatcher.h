#pragma once
#include <HAL/spinlock.h>

#ifdef __cplusplus
extern "C" {
#endif

    extern KSPIN_LOCK DispatcherLock;

    KIRQL
    NTAPI
    KiAcquireDispatcherLock();

    VOID
    NTAPI KiReleaseDispatcherLock(KIRQL oldIrql);

#ifdef __cplusplus
}
#endif
