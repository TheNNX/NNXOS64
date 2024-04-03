#define NNX_SPINLOCK_IMPL
#include <spinlock.h>
#include <bugcheck.h>
#include <pcr.h>
#include <cpu.h>
#include <SimpleTextIO.h>

VOID 
NTAPI 
KeAcquireSpinLockAtDpcLevel(
    PKSPIN_LOCK Lock)
{
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
    {
        KeBugCheckEx(
            IRQL_NOT_DISPATCH_LEVEL, 
            __LINE__,
            KeGetCurrentIrql(),
            0,
            0);
    }

    KiAcquireSpinLock((volatile ULONG_PTR*)Lock);
}

VOID 
NTAPI 
KeReleaseSpinLockFromDpcLevel(
    PKSPIN_LOCK Lock)
{
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
    {
        KeBugCheckEx(
            IRQL_NOT_DISPATCH_LEVEL, 
            __LINE__,
            KeGetCurrentIrql(),
            0, 
            0);
    }

    KiReleaseSpinLock((volatile ULONG_PTR*)Lock);
}

KIRQL 
FASTCALL 
KfAcquireSpinLock(
    PKSPIN_LOCK lock) 
{
    KIRQL temp = 0;
    KeRaiseIrql(DISPATCH_LEVEL, &temp);
    KiAcquireSpinLock((volatile ULONG_PTR*)lock);

    return temp;
}

VOID 
FASTCALL 
KfReleaseSpinLock(
    PKSPIN_LOCK lock, 
    KIRQL newIrql) 
{
    if (KeGetCurrentIrql() == PASSIVE_LEVEL)
    {
        KeBugCheckEx(
            IRQL_NOT_GREATER_OR_EQUAL, 
            __LINE__, 
            KeGetCurrentIrql(), 
            0, 
            0);
    }
    KiReleaseSpinLock((volatile ULONG_PTR*)lock);
    KeLowerIrql(newIrql);
}

VOID 
NTAPI 
KeAcquireSpinLock(
    PKSPIN_LOCK lock, 
    PKIRQL oldIrql) 
{    
    *oldIrql = KfAcquireSpinLock(lock);
}

VOID 
NTAPI 
KeReleaseSpinLock(
    PKSPIN_LOCK lock, 
    KIRQL newIrql) 
{
    KfReleaseSpinLock(lock, newIrql);
}

VOID 
NTAPI 
KeInitializeSpinLock(
    PKSPIN_LOCK lock)
{
#ifdef DESPERATE_SPINLOCK_DEBUG
    lock->Number = 0;
    lock->LockedByFunction = NULL;
    lock->LockedByLine = 0;
    lock->LockedByCore = 0;
#else
    *lock = 0;
#endif
}

typedef struct _SPINLOCK_DEBUG
{
    const char* LockName;
    const char* FunctionName;
    int Line;
    BOOLEAN Acquired;
    PKSPIN_LOCK Lock;
} SPINLOCK_DEBUG, *PSPINLOCK_DEBUG;

SPINLOCK_DEBUG SpinLockDebug[64] = { 0 };
KSPIN_LOCK LockLock = { 0 };

static
VOID
KiSpinlockDebug(
    KSPIN_LOCK* lock,
    const char* lockName,
    const char* functionName,
    int line,
    BOOLEAN acquired)
{
    extern UINT PspCoresInitialized;

    if (PspCoresInitialized >= KeNumberOfProcessors)
    {
        KiAcquireSpinLock((volatile ULONG_PTR*)&LockLock);

        PSPINLOCK_DEBUG debug = &SpinLockDebug[KeGetCurrentProcessorId()];
        debug->Line = line;
        debug->FunctionName = functionName;
        debug->LockName = lockName;
        debug->Acquired = acquired;
        debug->Lock = lock;

        KiReleaseSpinLock((volatile ULONG_PTR*)&LockLock);
    }
}

VOID
KiPrintSpinlockDebug()
{
    PKSPIN_LOCK lock;

    for (int i = 0; i < 16; i++)
    {
        if (SpinLockDebug[i].FunctionName != NULL &&
            SpinLockDebug[i].Acquired)
        {
            PrintT(
                "Core %i - [%s:%i] %s\n", 
                i, 
                SpinLockDebug[i].FunctionName,
                SpinLockDebug[i].Line,
                SpinLockDebug[i].LockName);

            lock = SpinLockDebug->Lock;
#ifdef DESPERATE_SPINLOCK_DEBUG
            if (lock && lock->LockedByFunction) 
            {
                PrintT("Acquired by:\n");
                PrintT(" - [Core %i - [%s:%i]]\n", lock->LockedByCore, lock->LockedByFunction, lock->LockedByLine);
            }           
            else if (lock)
            {
                PrintT("Lock value: %i\n", *(volatile ULONG_PTR*)lock);
            }
#endif
        }
    }
    PrintT("\n");
}

static
VOID
KiSetLock(
    PKSPIN_LOCK lock, const char* functionName, int line)
{
#ifdef DESPERATE_SPINLOCK_DEBUG
    extern UINT PspCoresInitialized;

    if (PspCoresInitialized >= KeNumberOfProcessors)
    {
        lock->LockedByFunction = functionName;
        lock->LockedByLine = line;
        lock->LockedByCore = KeGetCurrentProcessorId();
    }
#endif
}

KIRQL
FASTCALL
KfAcquireSpinLockDebug(
    PKSPIN_LOCK lock,
    const char* lockName,
    const char* functionName,
    int line)
{
    KIRQL kirql;

    KiSpinlockDebug(lock, lockName, functionName, line, TRUE);
    kirql = KfAcquireSpinLock(lock);
    KiSetLock(lock, functionName, line);
    return kirql;
}

VOID
FASTCALL
KfReleaseSpinLockDebug(
    PKSPIN_LOCK lock,
    KIRQL newIrql,
    const char* lockName,
    const char* functionName,
    int line)
{
    KiSpinlockDebug(lock, lockName, functionName, line, FALSE);
    KfReleaseSpinLock(lock, newIrql);
}

VOID
NTAPI
KeAcquireSpinLockDebug(
    PKSPIN_LOCK lock,
    PKIRQL oldIrql,
    const char* lockName,
    const char* functionName,
    int line)
{
    KiSpinlockDebug(lock, lockName, functionName, line, TRUE);
    KeAcquireSpinLock(lock, oldIrql);
    KiSetLock(lock, functionName, line);
}

VOID
NTAPI
KeReleaseSpinLockDebug(
    PKSPIN_LOCK lock,
    KIRQL newIrql,
    const char* lockName,
    const char* functionName,
    int line)
{
    KiSpinlockDebug(lock, lockName, functionName, line, FALSE);
    KeReleaseSpinLock(lock, newIrql);
}

VOID
NTAPI
KeAcquireSpinLockAtDpcLevelDebug(
    PKSPIN_LOCK lock,
    const char* lockName,
    const char* functionName,
    int line)
{
    KiSpinlockDebug(lock, lockName, functionName, line, TRUE);
    KeAcquireSpinLockAtDpcLevel(lock);
    KiSetLock(lock, functionName, line);
}

VOID
NTAPI
KeReleaseSpinLockFromDpcLevelDebug(
    PKSPIN_LOCK lock,
    const char* lockName,
    const char* functionName,
    int line)
{
    KiSpinlockDebug(lock, lockName, functionName, line, FALSE);
    KeReleaseSpinLockFromDpcLevel(lock);
}