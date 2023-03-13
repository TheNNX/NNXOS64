#include <HAL/cpu.h>
#include <HAL/X64/msr.h>
#include <HAL/pcr.h>
#include "APIC.h"

VOID 
NTAPI
HalSetPcr(PKPCR pcr)
{
	__writemsr(IA32_KERNEL_GS_BASE, (UINT64)NULL);
	__writemsr(IA32_GS_BASE, (UINT64)pcr);
}

ULONG
NTAPI
KeGetCurrentProcessorId()
{
	return (ULONG)ApicGetCurrentLapicId();
}