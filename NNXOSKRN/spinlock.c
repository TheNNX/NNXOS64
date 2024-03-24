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

    KiAcquireSpinLock(Lock);
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

    KiReleaseSpinLock(Lock);
}

KIRQL 
FASTCALL 
KfAcquireSpinLock(
    PKSPIN_LOCK lock) 
{
    KIRQL temp = 0;
    KeRaiseIrql(DISPATCH_LEVEL, &temp);
    KiAcquireSpinLock(lock);

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
    KiReleaseSpinLock(lock);
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
    *lock = 0;
}

typedef struct _SPINLOCK_DEBUG
{
    const char* LockName;
    const char* FunctionName;
    int Line;
    BOOLEAN Acquired;
} SPINLOCK_DEBUG, *PSPINLOCK_DEBUG;

SPINLOCK_DEBUG SpinLockDebug[64] = { 0 };
KSPIN_LOCK LockLock = 0;

static
VOID
KiSpinlockDebug(
    const char* lockName,
    const char* functionName,
    int line,
    BOOLEAN acquired)
{
    extern UINT PspCoresInitialized;

    if (PspCoresInitialized >= KeNumberOfProcessors)
    {
        KiAcquireSpinLock(&LockLock);

        PSPINLOCK_DEBUG debug = &SpinLockDebug[KeGetCurrentProcessorId()];
        debug->Line = line;
        debug->FunctionName = functionName;
        debug->LockName = lockName;
        debug->Acquired = acquired;

        KiReleaseSpinLock(&LockLock);
    }
}

VOID
KiPrintSpinlockDebug()
{
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
        }
    }
    PrintT("\n");
}

KIRQL
FASTCALL
KfAcquireSpinLockDebug(
    PKSPIN_LOCK lock,
    const char* lockName,
    const char* functionName,
    int line)
{
    KiSpinlockDebug(lockName, functionName, line, TRUE);
    return KfAcquireSpinLock(lock);
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
    KiSpinlockDebug(lockName, functionName, line, FALSE);
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
    KiSpinlockDebug(lockName, functionName, line, TRUE);
    KeAcquireSpinLock(lock, oldIrql);
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
    KiSpinlockDebug(lockName, functionName, line, FALSE);
    KeReleaseSpinLock(lock, newIrql);
}

VOID
NTAPI
KeAcquireSpinLockAtDpcLevelDebug(
    PKSPIN_LOCK Lock,
    const char* lockName,
    const char* functionName,
    int line)
{
    KiSpinlockDebug(lockName, functionName, line, TRUE);
    KeAcquireSpinLockAtDpcLevel(Lock);
}

VOID
NTAPI
KeReleaseSpinLockFromDpcLevelDebug(
    PKSPIN_LOCK Lock,
    const char* lockName,
    const char* functionName,
    int line)
{
    KiSpinlockDebug(lockName, functionName, line, FALSE);
    KeReleaseSpinLockFromDpcLevel(Lock);
}