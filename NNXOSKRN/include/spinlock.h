#ifndef NNX_SPINLOCK_HEADER
#define NNX_SPINLOCK_HEADER

#ifdef __cplusplus
extern "C" {
#endif
#include <nnxtype.h>
#include <irql.h>

#define DESPERATE_SPINLOCK_DEBUG

#pragma warning(push)
#pragma warning(disable: 4324)
#ifndef DESPERATE_SPINLOCK_DEBUG
    typedef ULONG_PTR volatile KSPIN_LOCK, * volatile PKSPIN_LOCK;
#define LOCKED(x) (x != 0)
#else
    typedef struct _KSPIN_LOCK_DEBUG
    {
        ULONG_PTR Number;
        const char* LockedByFunction;
        int LockedByLine;
        int LockedByCore;
    } volatile KSPIN_LOCK, * volatile PKSPIN_LOCK;
#define LOCKED(x) (x.Number != 0)
#endif
#pragma warning(pop)

#if defined(NNX_HAL) || defined(NNX_KERNEL)
    NTHALAPI
    VOID
    NTAPI
    KiAcquireSpinLock(
        volatile ULONG_PTR* SpinLock);

    NTHALAPI
    VOID
    NTAPI
    KiReleaseSpinLock(
        volatile ULONG_PTR* SpinLock);
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

#if defined(_DEBUG) && !defined(NNX_SPINLOCK_IMPL) 

    KIRQL
    FASTCALL
    KfAcquireSpinLockDebug(
        PKSPIN_LOCK lock,
        const char* lockName,
        const char* functionName,
        int line);

    VOID
    FASTCALL
    KfReleaseSpinLockDebug(
        PKSPIN_LOCK lock,
        KIRQL newIrql,
        const char* lockName,
        const char* functionName,
        int line);

    VOID
    NTAPI
    KeAcquireSpinLockDebug(
        PKSPIN_LOCK lock,
        PKIRQL oldIrql,
        const char* lockName,
        const char* functionName,
        int line);

    VOID
    NTAPI
    KeReleaseSpinLockDebug(
        PKSPIN_LOCK lock,
        KIRQL newIrql,
        const char* lockName,
        const char* functionName,
        int line);

    VOID
    NTAPI
    KeAcquireSpinLockAtDpcLevelDebug(
        PKSPIN_LOCK Lock,
        const char* lockName,
        const char* functionName,
        int line);

    VOID
    NTAPI
    KeReleaseSpinLockFromDpcLevelDebug(
        PKSPIN_LOCK Lock,
        const char* lockName,
        const char* functionName,
        int line);

#define XSTR(s) #s
#define STR(s) XSTR(s)

#define KfAcquireSpinLock(lock) \
KfAcquireSpinLockDebug(lock, #lock, __FUNCTION__, __LINE__)
#define KfReleaseSpinLock(lock, newIrql) \
KfReleaseSpinLockDebug(lock, newIrql, #lock, __FUNCTION__, __LINE__)
#define KeAcquireSpinLock(lock, oldIrql) \
KeAcquireSpinLockDebug(lock, oldIrql, #lock, __FUNCTION__, __LINE__)
#define KeReleaseSpinLock(lock, newIrql) \
KeReleaseSpinLockDebug(lock, newIrql, #lock, __FUNCTION__, __LINE__)
#define KeAcquireSpinLockAtDpcLevel(lock) \
KeAcquireSpinLockAtDpcLevelDebug(lock, #lock, __FUNCTION__, __LINE__)
#define KeReleaseSpinLockFromDpcLevel(lock) \
KeReleaseSpinLockFromDpcLevelDebug(lock, #lock, __FUNCTION__, __LINE__)
#undef STR

#endif

#ifdef __cplusplus
}
#endif

#endif
