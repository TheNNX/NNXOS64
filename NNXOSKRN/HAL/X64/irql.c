#include <HAL/irql.h>
#include "APIC.H"
#include <HAL/pcr.h>
#include <bugcheck.h>
#include "cpu.h"

static
VOID
HalX64SetTpr(
	UBYTE Value)
{
	SetCR8(Value);
}

KIRQL 
FASTCALL 
KfRaiseIrql(
	KIRQL NewIrql)
{
	KIRQL oldIrql = __readgsbyte(FIELD_OFFSET(KPCR, Irql));

	if (NewIrql < oldIrql)
	{
		KeBugCheckEx(
			IRQL_NOT_GREATER_OR_EQUAL,
			(ULONG_PTR)NewIrql,
			(ULONG_PTR)oldIrql,
			0,
			(ULONG_PTR)KfRaiseIrql);
	}

	HalX64SetTpr(NewIrql);

	__writegsbyte(FIELD_OFFSET(KPCR, Irql), NewIrql);
	return oldIrql;
}

VOID 
FASTCALL 
KfLowerIrql(
	KIRQL NewIrql)
{
	KIRQL oldIrql = __readgsbyte(FIELD_OFFSET(KPCR, Irql));

	if (NewIrql > oldIrql)
	{
		KeBugCheckEx(
			IRQL_NOT_LESS_OR_EQUAL,
			(ULONG_PTR)NewIrql,
			(ULONG_PTR)oldIrql,
			0,
			(ULONG_PTR)KfLowerIrql);
	}

	__writegsbyte(FIELD_OFFSET(KPCR, Irql), NewIrql);
	HalX64SetTpr(NewIrql);
}

VOID 
NTAPI 
KeRaiseIrql(
	KIRQL NewIrql, 
	PKIRQL OldIrql)
{
	if (OldIrql == (PKIRQL) NULL)
		return;
	*OldIrql = KfRaiseIrql(NewIrql);
}

VOID 
NTAPI 
KeLowerIrql(
	KIRQL OldIrql)
{
	KfLowerIrql(OldIrql);
}

KIRQL 
NTAPI 
KeGetCurrentIrql()
{
	return __readgsbyte(FIELD_OFFSET(KPCR, Irql));
}

KIRQL
KiSetIrql(
	KIRQL NewIrql)
{
	KIRQL oldIrql;

	oldIrql = __readgsbyte(FIELD_OFFSET(KPCR, Irql));
	__writegsbyte(FIELD_OFFSET(KPCR, Irql), NewIrql);
	HalX64SetTpr(NewIrql);

	return oldIrql;
}