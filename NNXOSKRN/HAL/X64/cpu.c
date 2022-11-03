#include "cpu.h"
#include "registers.h"
#include <HAL/pcr.h>
#include "APIC.h"

VOID HalSetPcr(PKPCR pcr)
{
	HalX64WriteMsr(IA32_KERNEL_GS_BASE, (UINT64)NULL);
	HalX64WriteMsr(IA32_GS_BASE, (UINT64)pcr);
}

ULONG_PTR KeGetCurrentProcessorId()
{
	return (ULONG_PTR)ApicGetCurrentLapicId();
}