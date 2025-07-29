#include <pool.h>
#include <pcr.h>
#include <SimpleTextIo.h>
#include <syscall.h>
#include <rtl.h>
#include <ntdebug.h>
#include <scheduler.h>

#define HalGetPcr() ((PKPCR)__readgsqword(0x18))
VOID HalSetPcr(PKPCR);
VOID HalpTaskSwitchHandler();
static KSPIN_LOCK PcrCreationLock = { 0 };


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
    PKARCH_CORE_DATA pCoreData;
    PKPCR pcr, tempPcr;

    KiAcquireSpinLock(&PcrCreationLock);
    HalDisableInterrupts();

    pCoreData = (PKARCH_CORE_DATA)PagingAllocatePageBlockFromRange(
        (sizeof(*pCoreData) + PAGE_SIZE - 1) / PAGE_SIZE,
        PAGING_KERNEL_SPACE,
        PAGING_KERNEL_SPACE_END);

    HalpInitializeIdt(
        pCoreData->IdtEntries,
        &pCoreData->Idtr);

    /* Loading gdt invalidates GS, it's necessary to restore temp PCR
     * otherwise, all subsequent IRQL changes (and because of that, 
     * spinlock uses) would fail. */
    tempPcr = KeGetPcr();

    HalpInitializeGdt(
        pCoreData->GdtEntires, 
        &pCoreData->Gdtr, 
        &pCoreData->Tss);

    HalSetPcr(tempPcr);
    pcr = HalCreatePcr(pCoreData->GdtEntires, pCoreData->IdtEntries, id);
    HalInitializeSystemCallForCurrentCore((ULONG_PTR)HalpSystemCall);
    KfRaiseIrql(DISPATCH_LEVEL);
    KiReleaseSpinLock(&PcrCreationLock);
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
    PETHREAD pDummyThread;
    PEPROCESS pDummyProcess;

    PKPRCB prcb = (PKPRCB) ExAllocatePool(NonPagedPool, sizeof(KPRCB));
    KeInitializeSpinLock(&prcb->Lock);
    
    pDummyProcess = (PEPROCESS)ExAllocatePool(NonPagedPool, sizeof(EPROCESS));
    pDummyThread = (PETHREAD) ExAllocatePool(NonPagedPool, sizeof(ETHREAD));
    
    pDummyProcess->Pcb.AddressSpace.TopStructPhysAddress = __readcr3();

    pDummyThread->Process     = pDummyProcess;
    pDummyThread->Tcb.Process = &pDummyProcess->Pcb;
    prcb->CurrentThread = &pDummyThread->Tcb;
    prcb->DummyThread   = &pDummyThread->Tcb;

    prcb->IdleThread = prcb->NextThread = (struct _KTHREAD*)NULL;
    prcb->Number = CoreNumber;

    prcb->DpcStack = NULL;
    prcb->DpcInProgress = FALSE;
    prcb->DpcEnding = FALSE;
    
    prcb->DpcData.DpcCount = 0;
    prcb->DpcData.DpcQueueDepth = 0;
    InitializeListHead(&prcb->DpcData.DpcListHead);
    KeInitializeSpinLock(&prcb->DpcData.DpcLock);

    return prcb;
}

PKPCR HalCreatePcr(PKGDTENTRY64 gdt, PKIDTENTRY64 idt, UCHAR CoreNumber)
{
    NTSTATUS status;
    PKPCR pcr = (PKPCR)ExAllocatePool(NonPagedPool, sizeof(KPCR));
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