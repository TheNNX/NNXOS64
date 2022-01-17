#include <HAL/irql.h>
#include <HAL/X64/APIC.H>
#include <HAL/x64/pcr.h>

/* Aparently, these FASTCALL versions are somewhat-often used and therefore I'll provide      */
KIRQL FASTCALL KfRaiseIrql(KIRQL newIrql)
{
	return NULL;
}

VOID FASTCALL KfLowerIrql(KIRQL newIrql)
{
	
}

VOID NTAPI KeRaiseIrql(KIRQL newIrql, PKIRQL oldIrql)
{
	if (oldIrql == (PKIRQL) NULL)
		return;
	*oldIrql = KfRaiseIrql(newIrql);
}

VOID NTAPI KeLowerIrql(KIRQL newIrql)
{
	KfLowerIrql(newIrql);
}

KIRQL KeGetCurrentIrql()
{
	return 0;
}