#include <irql.h>
#include <HAL/interrupt.h>
#include <HAL/pcr.h>
#include <bugcheck.h>
#include <HAL/cpu.h>

/* FIXME: It turns out, that masking IOAPIC interurpts isn't a good
 * idea after all - interrupts get ignored instead of put in IRR, and
 * in case of the keyboard interrupt for example, no new interrupt is sent
 * until there is data in the buffer. */
#define THE_OLD_RELIABLE
static
VOID
KiApplyIrql(
	KIRQL OldValue, 
	KIRQL NewValue)
{
#ifndef THE_OLD_RELIABLE
	PKPCR Pcr;
	PKINTERRUPT Interrupt;
	PLIST_ENTRY Entry;

	if (OldValue == NewValue)
		return;

	/* This *MAYBE* works. */
	HalSetTpr(0xF);

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

	HalSetTpr(0);
#else
	if (OldValue == NewValue)
		return;
	HalSetTpr(NewValue);
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