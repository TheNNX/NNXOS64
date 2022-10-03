#include "cmos.h"
#include <HAL/Port.h>
#include <HAL/spinlock.h>

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

    KeAcquireSpinLock(&CmosSpinlock, &irql);
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

    KeAcquireSpinLock(&CmosSpinlock, &irql);
    CmosSelectRegister(Register);
    returnValue = inb(0x71);
    KeReleaseSpinLock(&CmosSpinlock, irql);

    return returnValue;
}