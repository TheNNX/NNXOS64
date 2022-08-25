#include "IDT.h"
#include <SimpleTextIo.h>
#include "device/Keyboard.h"
#include "registers.h"
#include "GDT.h"
#include <HAL/paging.h>

VOID KeStop();

VOID DefExceptionHandler(UINT64 n, UINT64 errcode, UINT64 errcode2, UINT64 rip)
{
	PrintT("error: %x %x at RIP 0x%X\n\nRegisters:\nRAX %X  RBX %X  RCX %X  RDX %X\nRDI %X  RSI %X  RSP %X  RBP %X\nCR2 %X", n, errcode, rip,
		   GetRAX(), GetRBX(), GetRCX(), GetRDX(),
		   GetRDI(), GetRSI(), GetRSP(), GetRBP(),
		   GetCR2()
	);
}

VOID HalpDefInterruptHandler();
VOID(*gExceptionHandlerPtr) (UINT64 n, UINT64 errcode, UINT64 errcode2, UINT64 rip) = DefExceptionHandler;

void ExceptionHandler(UINT64 n, UINT64 errcode, UINT64 errcode2, UINT64 rip)
{
	gExceptionHandlerPtr(n, errcode, errcode2, rip);
	PrintT("ExHandler\n");
	KeStop();
}

ULONG HalpGetGdtBase(KGDTENTRY64 entry)
{
	return entry.Base0To15 | (entry.Base16To23 << 16UL) | (entry.Base24To31 << 24UL);
}

PKTSS HalpGetTssBase(KGDTENTRY64 tssEntryLow, KGDTENTRY64 tssEntryHigh)
{
	return (PKTSS)(HalpGetGdtBase(tssEntryLow) | ((*((UINT64*) &tssEntryHigh)) << 32UL));
}

VOID HalpSetIdtEntry(KIDTENTRY64* Idt, UINT64 EntryNo, PVOID Handler, BOOL Usermode, BOOL Trap)
{
	Idt[EntryNo].Selector = 0x8;
	Idt[EntryNo].Zero = 0;
	Idt[EntryNo].Offset0to15 = (UINT16) (((ULONG_PTR) Handler) & UINT16_MAX);
	Idt[EntryNo].Offset16to31 = (UINT16) ((((ULONG_PTR) Handler) >> 16) & UINT16_MAX);
	Idt[EntryNo].Offset32to63 = (UINT32) ((((ULONG_PTR) Handler) >> 32) & UINT32_MAX);
	Idt[EntryNo].Type = 0x8E | (Usermode ? (0x60) : 0x00) | (Trap ? 0 : 1);
	Idt[EntryNo].Ist = 0;
}


VOID HalpSetInterruptIst(KIDTENTRY64* Idt, UINT64 EntryNo, UCHAR Ist)
{
	Idt[EntryNo].Ist = Ist;
}

KIDTENTRY64* HalpAllocateAndInitializeIdt()
{
	const void(*interrupts[])() = { Exception0, Exception1, Exception2, Exception3,
								Exception4, Exception5, Exception6, Exception7,
								Exception8, ExceptionReserved, Exception10, Exception11,
								Exception12, Exception13, Exception14, ExceptionReserved,
								Exception16, Exception17, Exception18, Exception19,
								Exception20, ExceptionReserved, ExceptionReserved, ExceptionReserved,
								ExceptionReserved, ExceptionReserved, ExceptionReserved, ExceptionReserved,
								ExceptionReserved, ExceptionReserved, Exception30, ExceptionReserved,
								HalpTaskSwitchHandler, IRQ1, IRQ2, IRQ3, IRQ4, IRQ5, IRQ6, IRQ7,
								IRQ8, IRQ9, IRQ10, IRQ11, IRQ12, IRQ13, IRQ14
	};
	DisableInterrupts();
	KIDTR64* idtr = (KIDTR64*)PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END);
	PKIDTENTRY64 idt = (PKIDTENTRY64)((ULONG_PTR) idtr + sizeof(KIDTR64));

	idtr->Size = sizeof(KIDTENTRY64) * 128 - 1;
	idtr->Base = idt;

	for (int a = 0; a < 128; a++)
	{
		VOID(*handler)();
		handler = HalpDefInterruptHandler;

		if (a < sizeof(interrupts) / sizeof(*interrupts))
			handler = (VOID(*)())interrupts[a];

		HalpSetIdtEntry(idt, a, handler, FALSE, FALSE);
	}

	HalpLoadIdt(idtr);

	return idt;
}

VOID IrqHandler(UINT64 a)
{
	if (a == 1)
		KeyboardInterrupt();
}

/* TODO: move code below to GDT.c */

UINT64 HalpSetGdtEntry(LPKGDTENTRY64 gdt, UINT64 entryIndex, UINT32 base, UINT32 limit, UINT8 flags, UINT8 accessByte)
{
	gdt[entryIndex].Base0To15 = base & UINT16_MAX;
	gdt[entryIndex].Base16To23 = (base & 0xFF0000) >> 16;
	gdt[entryIndex].Base24To31 = (base & 0xFF000000) >> 24;
	gdt[entryIndex].Limit0To15 = limit & UINT16_MAX;
	gdt[entryIndex].Limit16To19 = (limit & 0xF0000) >> 16;
	gdt[entryIndex].Flags = flags & 0xF;
	gdt[entryIndex].AccessByte = accessByte;

	return sizeof(KGDTENTRY64) * entryIndex;
}

UINT64 HalpSetGdtTssDescriptorEntry(LPKGDTENTRY64 gdt, UINT64 entryIndex, PVOID tss, SIZE_T tssSize)
{
	HalpSetGdtEntry(gdt, entryIndex, ((ULONG_PTR)tss) & UINT32_MAX, (UINT32)tssSize, 0x40, 0x89);
	*((UINT64*) (gdt + entryIndex + 1)) = (((ULONG_PTR) tss) & ~((UINT64)UINT32_MAX)) >> 32;

	return sizeof(KGDTENTRY64) * entryIndex;
}

VOID HalpInitializeGdt(KGDTR64* gdtr)
{
	UINT64 null, code0, data0, code3, data3, tr;
	PKGDTENTRY64 gdt = (PKGDTENTRY64) gdtr->Base;
	KTSS* tss = (KTSS*)PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END);
	tss->IopbBase = sizeof(*tss);

	null = HalpSetGdtEntry(gdt, 0, 0x00000000UL, 0x00000UL, 0x0, 0x00);
	code0 = HalpSetGdtEntry(gdt, 1, 0x00000000UL, 0xFFFFFUL, 0xA, 0x9A);
	data0 = HalpSetGdtEntry(gdt, 2, 0x00000000UL, 0xFFFFFUL, 0xC, 0x92);
	code3 = HalpSetGdtEntry(gdt, 3, 0x00000000UL, 0xFFFFFUL, 0xA, 0xFA);
	data3 = HalpSetGdtEntry(gdt, 4, 0x00000000UL, 0xFFFFFUL, 0xC, 0xF2);
	tr = HalpSetGdtTssDescriptorEntry(gdt, 5, tss, sizeof(*tss));

	HalpLoadGdt(gdtr);
	HalpLoadTss(tr);
}

PKGDTENTRY64 HalpAllocateAndInitializeGdt()
{
	KGDTR64* gdtr = (KGDTR64*)PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END);
	KGDTENTRY64* gdt = (KGDTENTRY64*)((ULONG_PTR)gdtr + sizeof(KGDTR64));

	gdtr->Size = sizeof(KGDTENTRY64) * 7 - 1;
	gdtr->Base = gdt;

	HalpInitializeGdt(gdtr);
	return gdt;
}