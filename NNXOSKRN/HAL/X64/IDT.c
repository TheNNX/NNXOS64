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
