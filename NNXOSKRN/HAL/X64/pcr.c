#include <nnxalloc.h>
#include <HAL/pcr.h>
#include <SimpleTextIo.h>
#include <HAL/syscall.h>
#include <rtl/rtl.h>
#include <ntdebug.h>

PKPCR HalGetPcr();
VOID HalSetPcr(PKPCR);

KSPIN_LOCK PcrCreationLock = 0;

VOID HalpTaskSwitchHandler();

/**
* @brief For all the code that for example has to lock after initialization
* if not for this dummy PCR, access to the IRQL would page fault.
*
* The correct one is set before multiprocessing init.
*/
static KPCR dummyPcr;
static KPRCB dummyPrcb;

VOID HalpInitDummyPcr()
{
	RtlZeroMemory(&dummyPcr, sizeof(dummyPcr));
	RtlZeroMemory(&dummyPrcb, sizeof(dummyPrcb));
	dummyPcr.Irql = DISPATCH_LEVEL;
	dummyPcr.Prcb = NULL;
	dummyPcr.SelfPcr = &dummyPcr;
	dummyPcr.Prcb = &dummyPrcb;
	dummyPrcb.CurrentThread = NULL;
	InitializeListHead(&dummyPcr.InterruptListHead);
}

VOID HalpSetDummyPcr()
{
	HalSetPcr(&dummyPcr);
}

VOID HalpSetupPcrForCurrentCpu(UCHAR id)
{
	PKPCR pcr, tempPcr;
	PKIDTENTRY64 idt;
	PKGDTENTRY64 gdt;
	HalAcquireLockRaw(&PcrCreationLock);
	
    DisableInterrupts();
	idt = HalpAllocateAndInitializeIdt();

	/* loading gdt invalidates GS, it's necessary to restore temp PCR */
	/* otherwise, all subsequent IRQL changes (and because of that, spinlock uses) would fail */
	tempPcr = KeGetPcr();

	gdt = HalpAllocateAndInitializeGdt();

	HalSetPcr(tempPcr);
	pcr = HalCreatePcr(gdt, idt, id);
	HalInitializeSystemCallForCurrentCore();
    KfRaiseIrql(DISPATCH_LEVEL);
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
	NTSTATUS status;
	PKPCR pcr = (PKPCR) NNXAllocatorAlloc(sizeof(KPCR));
	pcr->Gdt = gdt;
	pcr->Idt = idt;
	pcr->Tss = HalpGetTssBase(pcr->Gdt[5], pcr->Gdt[6]);
	InitializeListHead(&pcr->InterruptListHead);

	pcr->Irql = 0;

	pcr->MajorVersion = 1;
	pcr->MinorVersion = 0;

	pcr->SelfPcr = pcr;
	
	pcr->Prcb = HalCreatePrcb(CoreNumber);
	HalSetPcr(pcr);
	KIRQL oldIrql = KfRaiseIrql(DISPATCH_LEVEL);
	status = KiInitializeInterrupts();
	ASSERT(NT_SUCCESS(status));
	KeLowerIrql(oldIrql);

	return pcr;
}