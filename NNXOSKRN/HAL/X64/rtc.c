#include <HAL/rtc.h>
#include <HAL/X64/cmos.h>
#include <HAL/spinlock.h>
#include <SimpleTextIO.h>

static UCHAR CenturyRegister;
static KSPIN_LOCK RtcLock;
static BOOLEAN RtcBcdMode;
static BOOLEAN Rtc12HMode;

VOID HalRtcInitialize(UCHAR CenturyRegisterNumber)
{
    UCHAR registerB;

    CenturyRegister = CenturyRegisterNumber;

    /* if no century register is given, guess */
    if (CenturyRegister == 0)
        CenturyRegister = 0x32;

    KeInitializeSpinLock(&RtcLock);

    CmosWriteRegister(0xB, CmosReadRegister(0x0B) & 0xF9);
    registerB = CmosReadRegister(0x0B);

    Rtc12HMode = !(registerB & 0x02);
    RtcBcdMode = !(registerB & 0x04);
}

static UCHAR HalpRtcHandleDataRead(UCHAR Register)
{
    UCHAR value;
    UCHAR value1, value2;
    KIRQL irql;

    KeAcquireSpinLock(&RtcLock, &irql);

    /**
     * read the values two times, until they're the same 
     * this is done to avoid reading when the RTC is updating
     */
    do
    {
        /* while update in progress */
        while (CmosReadRegister(0x0A) & 0x80);
        value1 = CmosReadRegister(Register);

        /* read for the second time */
        while (CmosReadRegister(0x0A) & 0x80);
        value2 = CmosReadRegister(Register);
    }
    while (value1 != value2);

    /* value1 and value2 are equal */
    value = value2;

    /* do BCD translation if neccessary */
    if (RtcBcdMode)
    {
        /* reause value1 and value2 for BCD digits */
        value1 = value & 0xF;
        value2 = (value & 0xF0) >> 4;

        value = 10 * value2 + value1;
    }

    KeReleaseSpinLock(&RtcLock, irql);
    return value;
}

UCHAR HalRtcGetSeconds()
{
    return HalpRtcHandleDataRead(0x00);
}

UCHAR HalRtcGetMinutes()
{
    return HalpRtcHandleDataRead(0x02);
}

UCHAR HalRtcGetHours()
{
    UCHAR value;
    value = HalpRtcHandleDataRead(0x04);
    
    if (!Rtc12HMode)
        return value;

    /**
     * Adjust for 12 hour mode 
     * Note: this is NOT done to DISPLAY different hour formats
     * It is done to READ them from the RTC. Any cosmetic changes to the way
     * the date and time are displayed, should be implemented in higher abstraction
     * parts of the system.
     */

    /**
     * According to the OSDev Wiki, in the 12 hour mode, the value 12 represents midnights,
     * so it should be converted to 0.
     */
    if (value == 12)
        return 0;

    /**
     * If the value is larger than 12, it means that the bit 7 in hours register was set
     * (this bit is the only possible cause of it being larger than the mode allows).
     * If so, it means that the hour count should be adjusted. The adjustment, however,
     * depends on the fact if the BCD mode is on.
     */
    if (value > 12)
    {
        if (RtcBcdMode)
        {
            /**
             * In BCD mode, if the hour register has bit 7 set, the reading of BCD value
             * This bit being set before chaning BCD to binary, appears as the value
             * being incremented by 80. That way, we can simply subtract (80 - 12) from
             * the hour count to get the correct value.
             */
            return value - (80 - 12);
        }
        else
        {
            /**
             * If the RTC is not in BCD mode, that means that the bit 7 being set manifests
             * itself by incrementing the value by 128. This can be dealt with by subtracting
             * (128 - 12)
             */
            return value - (128 - 12);
        }
    }

    return value;
}

UCHAR HalRtcGetDay()
{
    return HalpRtcHandleDataRead(0x07);
}

UCHAR HalRtcGetMonth()
{
    return HalpRtcHandleDataRead(0x08);
}

USHORT HalRtcGetYear()
{
    if (CenturyRegister != 0)
        return HalpRtcHandleDataRead(0x09) + 100 * HalpRtcHandleDataRead(CenturyRegister);
    return HalpRtcHandleDataRead(0x09);
}

static VOID PrintDigits(UCHAR Digits, ULONG_PTR Value)
{
    ULONG_PTR valueCopy;
    ULONG i;

    valueCopy = Value;

    for (i = 0; i < Digits; i++)
    {
        if (!valueCopy)
        {
            PrintT("0");
        }
        valueCopy /= 10;
    }

    PrintT("%i", Value);
}

VOID HalpPrintCurrentTime()
{
    PrintDigits(2, (ULONG_PTR)HalRtcGetHours());
    PrintT(":");
    PrintDigits(2, (ULONG_PTR)HalRtcGetMinutes());
    PrintT(":");
    PrintDigits(2, (ULONG_PTR)HalRtcGetSeconds());
}

VOID HalpPrintCurrentDate()
{
    PrintDigits(2, (ULONG_PTR)HalRtcGetDay());
    PrintT(".");
    PrintDigits(2, (ULONG_PTR)HalRtcGetMonth());
    PrintT(".%i", (ULONG_PTR)HalRtcGetYear());
}