#include <nnxalloc.h>
#include <HAL/pcr.h>
#include <SimpleTextIo.h>

PKPCR HalGetPcr();
VOID HalSetPcr(PKPCR);

KSPIN_LOCK PcrCreationLock = 0;

VOID HalpTaskSwitchHandler();

VOID HalpSetupPcrForCurrentCpu(UCHAR id)
{
	PKPCR pcr;
	PKIDTENTRY64 idt;
	PKGDTENTRY64 gdt;
	HalAcquireLockRaw(&PcrCreationLock);
	DisableInterrupts();
	gdt = HalpAllocateAndInitializeGdt();
	idt = HalpAllocateAndInitializeIdt();
	// HalpSetIdtEntry(idt, 0x20, HalpTaskSwitchHandler, TRUE, FALSE);

	pcr = HalCreatePcr(gdt, idt, id);
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

PKPRCB HalCreatePrcb(UCHAR CoreNumber)
{
	PKPRCB prcb = (PKPRCB) NNXAllocatorAlloc(sizeof(KPRCB));
	KeInitializeSpinLock(&prcb->Lock);
	prcb->CurrentThread = prcb->IdleThread = prcb->NextThread = (struct _KTHREAD*)NULL;
	prcb->Number = CoreNumber;

	return prcb;
}

PKPCR HalCreatePcr(PKGDTENTRY64 gdt, PKIDTENTRY64 idt, UCHAR CoreNumber)
{
	PKPCR pcr = (PKPCR) NNXAllocatorAlloc(sizeof(KPCR));

	pcr->Gdt = gdt;
	pcr->Idt = idt;
	pcr->Tss = HalpGetTssBase(pcr->Gdt[5], pcr->Gdt[6]);
	PrintT("Set pcr->Tss to %x\n", pcr->Tss);

	pcr->Irql = 0;

	pcr->MajorVersion = 1;
	pcr->MinorVersion = 0;

	pcr->SelfPcr = pcr;

	pcr->Prcb = HalCreatePrcb(CoreNumber);

	HalSetPcr(pcr);

	return pcr;
}