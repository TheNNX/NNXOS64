#include "irql.h"
#include "APIC/APIC.h"

KIRQL FASTCALL KfRaiseIrql(KIRQL newIrql) 
{
	
}

VOID FASTCALL KfLowerIrql(KIRQL newIrql) 
{
	
}

VOID NTAPI KeRaiseIrql(KIRQL newIrql, PKIRQL oldIrql) 
{
	if (oldIrql == (PKIRQL)NULL)
		return;
	*oldIrql = KfRaiseIrql(newIrql);
}

VOID NTAPI KeLowerIrql(KIRQL newIrql) 
{
	KfLowerIrql(newIrql);
}