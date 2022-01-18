#include <HAL/irql.h>
#include "APIC.H"
#include <HAL/pcr.h>

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