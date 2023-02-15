#pragma once
#include <HAL/spinlock.h>

extern KSPIN_LOCK DispatcherLock;
KIRQL KiAcquireDispatcherLock();
VOID KiReleaseDispatcherLock(KIRQL oldIrql);