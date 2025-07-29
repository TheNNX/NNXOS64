#include <time.h>
#include <SimpleTextIO.h>
#include <HALX64/include/rtc.h>
#include <dispatcher.h>
#include <scheduler.h>
#include <ntdebug.h>

extern LIST_ENTRY RelativeTimeoutListHead;
extern LIST_ENTRY AbsoluteTimeoutListHead;
extern ULONG_PTR KeMaximumIncrement;

ULONG64 KiCyclesPerClockQuantum;
ULONG64 KiCyclesPerQuantum;
ULONG64 KiTicksPerSecond;
ULONG64 KiClockTickInterval;

volatile ULONG64 KeClockTicks = 0;
static volatile ULONG64 KeLastRtcSyncTime = 0;
static volatile ULONG64 KeCurrentRtc = 0;

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

VOID
NTAPI
KiPrintCurrentTime()
{
    PrintDigits(2, (ULONG_PTR)HalRtcGetHours());
    PrintT(":");
    PrintDigits(2, (ULONG_PTR)HalRtcGetMinutes());
    PrintT(":");
    PrintDigits(2, (ULONG_PTR)HalRtcGetSeconds());
}

VOID
NTAPI
KiPrintCurrentDate()
{
    PrintDigits(2, (ULONG_PTR)HalRtcGetDay());
    PrintT(".");
    PrintDigits(2, (ULONG_PTR)HalRtcGetMonth());
    PrintT(".%i", (ULONG_PTR)HalRtcGetYear());
}

NTSYSAPI
VOID
NTAPI
KeQuerySystemTime(PULONG64 CurrentTime)
{
    ULONG64 rtcTime;
    ULONG64 tickTime = KeClockTicks;
    ULONG64 ticksPerSecond = 10'000'000 / KiClockTickInterval;

    const ULONG64 syncAfterSeconds = 3;

    if (KeGetCurrentProcessorId() == 0)
    {
        if (tickTime - KeLastRtcSyncTime > syncAfterSeconds * ticksPerSecond)
        {
            HalRtcGetTime(&rtcTime);
            KeCurrentRtc = rtcTime;

            if (tickTime - KeLastRtcSyncTime != syncAfterSeconds * ticksPerSecond)
            {
                PrintT("Time synced to %i(%it/%is, after %it)\n",
                       KeCurrentRtc, 
                       syncAfterSeconds * ticksPerSecond,
                       syncAfterSeconds,
                       tickTime - KeLastRtcSyncTime);
            }
            KeLastRtcSyncTime = tickTime;
        }
    }

    *CurrentTime = KeCurrentRtc + (tickTime - KeLastRtcSyncTime) * KiClockTickInterval;
}

static
VOID
KiExpireTimeout(PKTIMEOUT_ENTRY pTimeout)
{
    RemoveEntryList(&pTimeout->ListEntry);
    if (pTimeout->OnTimeout != NULL)
    {
        pTimeout->OnTimeout(pTimeout);
    }
}

VOID
NTAPI
KiClockTick()
{
    PLIST_ENTRY Current;
    PKTIMEOUT_ENTRY TimeoutEntry;
    ULONG64 Time = 0;

    if (KeGetCurrentProcessorId() == 0)
    {
        _InterlockedIncrement64(&KeClockTicks);
    }

    ASSERT(LOCKED(DispatcherLock));

#if 1
    KeQuerySystemTime(&Time);

    Current = AbsoluteTimeoutListHead.First;
    while (Current != &AbsoluteTimeoutListHead)
    {
        TimeoutEntry = CONTAINING_RECORD(Current, KTIMEOUT_ENTRY, ListEntry);
        Current = Current->Next;

        if (TimeoutEntry->Timeout > Time)
        {
            KiExpireTimeout(TimeoutEntry);
        }
    }
#endif

    Current = RelativeTimeoutListHead.First;
    while (Current != &RelativeTimeoutListHead)
    {
        TimeoutEntry = CONTAINING_RECORD(Current, KTIMEOUT_ENTRY, ListEntry);
        Current = Current->Next;

        /* Temporary solution. */
        if (KeGetCurrentProcessorId() == 0)
        {
            TimeoutEntry->Timeout -= KeMaximumIncrement;
        }
        if ((LONG64)TimeoutEntry->Timeout < 0)
        {
            KiExpireTimeout(TimeoutEntry);
        }
    }
}
VOID
NTAPI
KiHandleObjectWaitTimeout(PKTHREAD Thread,
                          PLONG64 pTimeout)
{
    ASSERT(LOCKED(DispatcherLock));

    if (Thread->TimeoutEntry.Timeout != 0)
    {
        RemoveEntryList(&Thread->TimeoutEntry.ListEntry);
    }

    Thread->TimeoutEntry.Timeout = 0;
    Thread->TimeoutEntry.TimeoutIsAbsolute = TRUE;

    if (pTimeout != NULL)
    {
        /* Relative is negative */
        if (*pTimeout < 0)
        {
            Thread->TimeoutEntry.TimeoutIsAbsolute = FALSE;
            Thread->TimeoutEntry.Timeout = -*pTimeout;
            InsertTailList(
                &RelativeTimeoutListHead,
                &Thread->TimeoutEntry.ListEntry);
        }
        /* Absolute is positive */
        else
        {
            Thread->TimeoutEntry.TimeoutIsAbsolute = TRUE;
            Thread->TimeoutEntry.Timeout = *pTimeout;
            InsertTailList(
                &AbsoluteTimeoutListHead,
                &Thread->TimeoutEntry.ListEntry);
        }
    }
}