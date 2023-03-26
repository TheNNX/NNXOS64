#include <irql.h>
#include <interrupt.h>
#include <pcr.h>
#include <bugcheck.h>
#include <cpu.h>

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

	__writegsbyte(FIELD_OFFSET(KPCR, Irql), NewValue);
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
	__writegsbyte(FIELD_OFFSET(KPCR, Irql), NewValue);
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
			(ULONG_PTR)_ReturnAddress());
	}

	KiApplyIrql(OldIrql, NewIrql);
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
			(ULONG_PTR)_ReturnAddress());
	}

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
	if (NewIrql < KeGetCurrentIrql())
	{
		KeBugCheckEx(
			IRQL_NOT_GREATER_OR_EQUAL,
			(ULONG_PTR)NewIrql,
			(ULONG_PTR)KeGetCurrentIrql(),
			0,
			(ULONG_PTR)_ReturnAddress());
	}
	*OldIrql = KfRaiseIrql(NewIrql);
}

VOID 
NTAPI 
KeLowerIrql(
	KIRQL OldIrql)
{
	if (OldIrql > KeGetCurrentIrql())
	{
		KeBugCheckEx(
			IRQL_NOT_LESS_OR_EQUAL,
			(ULONG_PTR)OldIrql,
			(ULONG_PTR)KeGetCurrentIrql(),
			0,
			(ULONG_PTR)_ReturnAddress());
	}
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
	KiApplyIrql(OldIrql, NewIrql);

	return OldIrql;
}