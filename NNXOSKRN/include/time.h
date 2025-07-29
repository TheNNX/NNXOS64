#pragma once

#include <nnxtype.h>

extern ULONG64 KiCyclesPerClockQuantum;
extern ULONG64 KiCyclesPerQuantum;
extern ULONG64 KiTicksPerSecond;
extern ULONG64 KiClockTickInterval;
extern volatile ULONG64 KeClockTicks;

#ifdef __cplusplus
extern "C"
{
#endif

VOID 
NTAPI
KiPrintCurrentTime();
    
VOID 
NTAPI
KiPrintCurrentDate();

NTSYSAPI
VOID
NTAPI
KeQuerySystemTime(PULONG64 CurrentTime);

#ifdef __cplusplus
}
#endif