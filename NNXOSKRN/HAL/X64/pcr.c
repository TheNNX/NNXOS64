#include <memory/nnxalloc.h>
#include "pcr.h"
#include <HAL/GDT.h>
#include <HAL/IDT.h>
#include <video/SimpleTextIo.h>

PKPCR HalGetPcr();
VOID HalSetPcr(PKPCR);

VOID HalAcquireLockRaw(UINT64* lock);
VOID HalReleaseLockRaw(UINT64* lock);

__declspec(align(64)) UINT64 PcrCreationLock = 0;

VOID HalpTaskSwitchHandler();

VOID HalpSetupPcrForCurrentCpu(UINT64 id)
{
	PKPCR pcr;
	PKIDTENTRY64 idt;
	PKGDTENTRY64 gdt;
	gdt = HalpAllocateAndInitializeGdt();
	idt = HalpAllocateAndInitializeIdt();
	// HalpSetIdtEntry(idt, 0x20, HalpTaskSwitchHandler, TRUE, FALSE);

	DisableInterrupts();
	HalAcquireLockRaw(&PcrCreationLock);
	pcr = HalCreatePcr(gdt, idt);
	HalReleaseLockRaw(&PcrCreationLock);
}

PKIDTENTRY64 HalpGetIdt()
{
	return HalGetPcr()->Idt;
}

PKTSS HalpGetTss()
{
	return HalGetPcr()->Tss;
}

PKGDTENTRY64 HalpGetGdt()
{
	return HalGetPcr()->Gdt;
}

PKPCR KeGetPcr()
{
	return HalGetPcr();
}

PKPCR HalCreatePcr(PKGDTENTRY64* gdt, PKIDTENTRY64* idt)
{
	PKPCR pcr = (PKPCR) NNXAllocatorAlloc(sizeof(KPCR));


	pcr->Gdt = gdt;
	pcr->Idt = idt;
	pcr->Tss = HalpGetTssBase(pcr->Gdt[5], pcr->Gdt[6]);
	PrintT("Set pcr->Tss to %x", pcr->Tss);

	pcr->Irql = 0;

	pcr->MajorVersion = 1;
	pcr->MinorVersion = 0;

	pcr->SelfPcr = pcr;

	/* TODO pcr->Prcb = HalCreatePcrb(); */
	pcr->Prcb = NULL;

	HalSetPcr(pcr);

	return pcr;
}