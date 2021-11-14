#include "IDT.h"
#include "video/SimpleTextIo.h"
#include "device/Keyboard.h"
#include "registers.h"
#include "GDT.h"
#include "../memory/physical_allocator.h"

BOOL HalpInteruptInitialized;


VOID DefHandler(UINT64 n, UINT64 errcode, UINT64 errcode2, UINT64 rip)
{
	PrintT("error: %x %x at RIP 0x%X\n\nRegisters:\nRAX %X  RBX %X  RCX %X  RDX %X\nRDI %X  RSI %X  RSP %X  RBP %X\nCR2 %X", n, errcode, rip,
		   GetRAX(), GetRBX(), GetRCX(), GetRDX(),
		   GetRDI(), GetRSI(), GetRSP(), GetRBP(),
		   GetCR2()
	);
}

VOID(*gExceptionHandlerPtr) (UINT64 n, UINT64 errcode, UINT64 errcode2, UINT64 rip) = DefHandler;

void ExceptionHandler(UINT64 n, UINT64 errcode, UINT64 errcode2, UINT64 rip)
{
	gExceptionHandlerPtr(n, errcode, errcode2, rip);
	KeStop();
}

void IrqHandler(UINT32 n)
{
	if (n == 1)
	{
		UINT8 character = KeyboardInterrupt();
		switch (character)
		{
			case 0:
				return;
			default:
				PrintT("%c", character);
				return;
				/* TODO: AddKeyToKeyboardBuffer(character); */
		};
	}
}

void IntTestASM();

ULONG HalpGetGdtBase(KGDTENTRY64 entry)
{
	return entry.Base0To15 | (entry.Base16To23 << 16UL) | (entry.Base24To31 << 24UL);
}

ULONG_PTR HalpGetTssBase(KGDTENTRY64 tssEntryLow, KGDTENTRY64 tssEntryHigh)
{
	return HalpGetGdtBase(tssEntryLow) | ((*((UINT64*) &tssEntryHigh)) << 32UL);
}

VOID PrintIdt(PKIDTENTRY64 idt, int to)
{
	for (int entryNo = 0; entryNo < to; entryNo++)
		PrintT("IDT[%i] (0x%X) set: %X 0x%X\n", 
			   entryNo, 
			   idt + entryNo, 
			   (UINT64) idt[entryNo].Type, 
			   ((UINT64)idt[entryNo].Offset0to15) | (((UINT64)idt[entryNo].Offset16to31) << 16ULL) | (((UINT64)idt[entryNo].Offset32to63) << 32ULL)
		);
}

VOID HalpSetIdtEntry(KIDTENTRY64* idt, UINT64 entryNo, PVOID handler, BOOL usermode, BOOL trap)
{
	idt[entryNo].Selector = 0x8;
	idt[entryNo].Zero = 0;
	idt[entryNo].Offset0to15 = (UINT16) (((ULONG_PTR) handler) & UINT16_MAX);
	idt[entryNo].Offset16to31 = (UINT16) ((((ULONG_PTR) handler) >> 16) & UINT16_MAX);
	idt[entryNo].Offset32to63 = (UINT32) ((((ULONG_PTR) handler) >> 32) & UINT32_MAX);
	idt[entryNo].Type = 0x8E | (usermode ? (0x60) : 0x00) | (trap ? 0 : 1);
	idt[entryNo].Ist = 0;
}

VOID Irq0C()
{
	PrintT("Timer tick\n");
}

KIDTENTRY64* HalpAllocateAndInitializeIdt()
{
	void(*interrupts[])() = { Exception0, Exception1, Exception2, Exception3,
								Exception4, Exception5, Exception6, Exception7,
								Exception8, ExceptionReserved, Exception10, Exception11,
								Exception12, Exception13, Exception14, ExceptionReserved,
								Exception16, Exception17, Exception18, Exception19,
								Exception20, ExceptionReserved, ExceptionReserved, ExceptionReserved,
								ExceptionReserved, ExceptionReserved, ExceptionReserved, ExceptionReserved,
								ExceptionReserved, ExceptionReserved, Exception30, ExceptionReserved,
								IRQ0, IRQ1, IRQ2, IRQ3, IRQ4, IRQ5, IRQ6, IRQ7,
								IRQ8, IRQ9, IRQ10, IRQ11, IRQ12, IRQ13, IRQ14
	};
	KIDTR64* idtr = PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END);
	KIDTENTRY64* idt = (((ULONG_PTR) idtr) + sizeof(KIDTR64));

	idtr->Size = sizeof(KIDTENTRY64) * 128 - 1;
	idtr->Base = idt;

	for (int a = 0; a < 128; a++)
	{
		VOID(*handler)();
		handler = IntTestASM;

		if (a < sizeof(interrupts) / sizeof(*interrupts))
			handler = interrupts[a];

		HalpSetIdtEntry(idt, a, handler, FALSE, FALSE);
	}

	HalpLoadIdt(idtr);

	HalpInteruptInitialized = TRUE;
	return idt;
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
	HalpSetGdtEntry(gdt, entryIndex, ((ULONG_PTR)tss) & UINT32_MAX, tssSize, 0x40, 0x89);
	*((UINT64*) (gdt + entryIndex + 1)) = (((ULONG_PTR) tss) & ~((UINT64)UINT32_MAX)) >> 32;

	return sizeof(KGDTENTRY64) * entryIndex;
}

VOID HalpInitializeGdt(KGDTR64* gdtr)
{
	UINT64 null, code0, data0, code3, data3, tr;
	PKGDTENTRY64 gdt = (PKGDTENTRY64) gdtr->Base;
	KTSS* tss = PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END);
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
	KGDTR64* gdtr = PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END);
	KGDTENTRY64* gdt = (((ULONG_PTR)gdtr) + sizeof(KGDTR64));

	gdtr->Size = sizeof(KGDTENTRY64) * 7 - 1;
	gdtr->Base = gdt;

	HalpInitializeGdt(gdtr);
	return gdt;
}