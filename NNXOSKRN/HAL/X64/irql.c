#include <HAL/irql.h>
#include <HAL/interrupt.h>
#include "APIC.H"
#include <HAL/pcr.h>
#include <bugcheck.h>
#include "cpu.h"

static
VOID
KiApplyIrql(
	KIRQL OldValue, 
	KIRQL NewValue)
{
	PKPCR Pcr;
	PKINTERRUPT Interrupt;
	PLIST_ENTRY Entry;

	if (OldValue == NewValue)
		return;
#ifndef THE_OLD_RELIABLE
	/* This *MAYBE* works. */
	SetCR8(0xF);

	Pcr = KeGetPcr();
	Entry = Pcr->InterruptListHead.First;

	while (Entry != &Pcr->InterruptListHead)
	{
		KIRQL InterruptIrql;
		Interrupt = CONTAINING_RECORD(Entry, KINTERRUPT, CpuListEntry);
		InterruptIrql = Interrupt->InterruptIrql;

		if (Interrupt->pfnSetMask == NULL)
		{
			Entry = Entry->Next;
			continue;
		}

		if (NewValue > OldValue)
		{
			if (InterruptIrql > OldValue &&
				InterruptIrql <= NewValue)
			{
				Interrupt->pfnSetMask(Interrupt, TRUE);
			}
		}
		
		else if (OldValue > NewValue)
		{
			if (InterruptIrql > NewValue &&
				InterruptIrql <= OldValue)
			{
				Interrupt->pfnSetMask(Interrupt, FALSE);
			}
		}

		Entry = Entry->Next;
	}

	SetCR8(NewValue);
#else
	SetCR8(NewValue);
#endif
}

KIRQL 
FASTCALL 
KfRaiseIrql(
	KIRQL NewIrql)
{
	KIRQL OldIrql = __readgsbyte(FIELD_OFFSET(KPCR, Irql));

	if (NewIrql < OldIrql)
	{
		KeBugCheckEx(
			IRQL_NOT_GREATER_OR_EQUAL,
			(ULONG_PTR)NewIrql,
			(ULONG_PTR)OldIrql,
			0,
			(ULONG_PTR)KfRaiseIrql);
	}

	KiApplyIrql(OldIrql, NewIrql);

	__writegsbyte(FIELD_OFFSET(KPCR, Irql), NewIrql);
	return OldIrql;
}

VOID 
FASTCALL 
KfLowerIrql(
	KIRQL NewIrql)
{
	KIRQL OldIrql = __readgsbyte(FIELD_OFFSET(KPCR, Irql));

	if (NewIrql > OldIrql)
	{
		KeBugCheckEx(
			IRQL_NOT_LESS_OR_EQUAL,
			(ULONG_PTR)NewIrql,
			(ULONG_PTR)OldIrql,
			0,
			(ULONG_PTR)KfLowerIrql);
	}

	__writegsbyte(FIELD_OFFSET(KPCR, Irql), NewIrql);
	KiApplyIrql(OldIrql, NewIrql);
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
	KIRQL OldIrql;

	OldIrql = __readgsbyte(FIELD_OFFSET(KPCR, Irql));
	__writegsbyte(FIELD_OFFSET(KPCR, Irql), NewIrql);
	KiApplyIrql(OldIrql, NewIrql);

	return OldIrql;
}