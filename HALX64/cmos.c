#include <cmos.h>
#include <Port.h>
#include <spinlock.h>
#include <rtc.h>

BOOLEAN HalNmiDesiredState = TRUE;
KSPIN_LOCK CmosSpinlock;

static VOID CmosSelectRegister(UCHAR Register)
{
    INT i;

    outb(0x70, Register | (HalNmiDesiredState << 7));
    /* I/O delay */
    for (i = 0; i < 1000; i++);
}

NTHALAPI
VOID 
NTAPI
CmosInitialize(UINT8 century)
{
    KeInitializeSpinLock(&CmosSpinlock);
    HalRtcInitialize(century);
}

VOID CmosWriteRegister(
    UCHAR Register,
    UCHAR Value
)
{
    KIRQL irql;

    KeRaiseIrql(HIGH_LEVEL, &irql);
    KiAcquireSpinLock(&CmosSpinlock);

    CmosSelectRegister(Register);
    outb(0x71, Value);
    KiReleaseSpinLock(&CmosSpinlock);
    KeLowerIrql(irql);
}

UCHAR CmosReadRegister(UCHAR Register)
{
    KIRQL irql;
    UCHAR returnValue;

    KeRaiseIrql(HIGH_LEVEL, &irql);
    KiAcquireSpinLock(&CmosSpinlock);

    CmosSelectRegister(Register);
    returnValue = inb(0x71);

    KiReleaseSpinLock(&CmosSpinlock);
    KeLowerIrql(irql);
    return returnValue;
}