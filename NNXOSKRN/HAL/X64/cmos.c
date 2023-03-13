#include "cmos.h"
#include <HAL/Port.h>
#include <spinlock.h>

BOOLEAN HalNmiDesiredState = TRUE;
KSPIN_LOCK CmosSpinlock;

static VOID CmosSelectRegister(UCHAR Register)
{
    INT i;

    outb(0x70, Register | (HalNmiDesiredState << 7));
    /* I/O delay */
    for (i = 0; i < 1000; i++);
}

VOID CmosInitialize()
{
    KeInitializeSpinLock(&CmosSpinlock);
}

VOID CmosWriteRegister(
    UCHAR Register,
    UCHAR Value
)
{
    KIRQL irql;

    irql = KeGetCurrentIrql();
    if (irql < DISPATCH_LEVEL)
        KeRaiseIrql(DISPATCH_LEVEL, &irql);
    KeAcquireSpinLockAtDpcLevel(&CmosSpinlock);

    CmosSelectRegister(Register);
    outb(0x71, Value);
    KeReleaseSpinLock(&CmosSpinlock, irql);
}

UCHAR CmosReadRegister(
    UCHAR Register
)
{
    KIRQL irql;
    UCHAR returnValue;

    irql = KeGetCurrentIrql();
    if (irql < DISPATCH_LEVEL)
        KeRaiseIrql(DISPATCH_LEVEL, &irql);
    KeAcquireSpinLockAtDpcLevel(&CmosSpinlock);

    CmosSelectRegister(Register);
    returnValue = inb(0x71);

    KeReleaseSpinLock(&CmosSpinlock, irql);

    return returnValue;
}