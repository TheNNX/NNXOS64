#include <HALX64/include/msr.h>
#include <pcr.h>
#include <cpu.h>
#include <HALX64/include/APIC.h>

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