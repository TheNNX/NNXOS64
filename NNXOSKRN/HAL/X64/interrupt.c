#include <HALX64/include/IDT.h>
#include <SimpleTextIo.h>
#include <Keyboard.h>
#include <HALX64/include/GDT.h>
#include <paging.h>
#include <dispatcher.h>
#include <scheduler.h>
#include <pool.h>
#include <ntdebug.h>
#include <pcr.h>
#include <HALX64/include/APIC.h>
#include <apc.h>

VOID 
HalpMockupInterruptHandler(
    ULONG_PTR Handler);

static
KIRQL
KiApplyInterruptIrql(
    PKINTERRUPT Interrupt);

static
KIRQL
KiApplyIrql(
    KIRQL irqls);

static
inline
VOID
InitializeInterrupt(
    PKINTERRUPT pInterrupt,
    UCHAR Vector,
    PVOID Handler,
    ULONG CpuNumber,
    KIRQL Irql,
    KSERVICE_ROUTINE Routine)
{
    pInterrupt->Vector = Vector;

    HalpInitInterruptHandlerStub(pInterrupt, (ULONG_PTR)Handler);

    pInterrupt->CpuNumber = CpuNumber;
    pInterrupt->InterruptIrql = Irql;
    pInterrupt->Trap = FALSE;
    pInterrupt->pfnFullCtxRoutine = NULL;
    pInterrupt->pfnServiceRoutine = Routine;
    pInterrupt->pfnGetRequiresFullCtx = NULL;
    pInterrupt->SendEOI = TRUE;
    pInterrupt->pfnSetMask = NULL;
    /* FIXME */
    pInterrupt->IoApicVector = Vector;
}

/* TODO: Create some sort of a KiDeleteInterrupt function. */
NTSTATUS
NTAPI
KiCreateInterrupt(
    PKINTERRUPT* ppInterrupt)
{
    PKINTERRUPT Interrupt = ExAllocatePool(NonPagedPool, sizeof(*Interrupt));

    if (Interrupt == NULL)
    {
        return STATUS_NO_MEMORY;
    }

    Interrupt->Connected = FALSE;
    KeInitializeSpinLock(&Interrupt->Lock);
    *ppInterrupt = Interrupt;

    return STATUS_SUCCESS;
}

static
BOOLEAN
GetRequiresFullCtxAlways(KINTERRUPT* interrupt)
{
    return TRUE;
}

NTSTATUS
NTAPI
KiInitializeInterrupts(VOID)
{
    ULONG i;
    ULONG cpuNumber = KeGetCurrentProcessorId();
    PKINTERRUPT clockInterrupt, stopInterrupt;
    NTSTATUS status;

    /* TODO: fix all of these handlers, so they can use the new
     * handling mode. */
    /* TODO: This should be moved to HAL maybe. */
    void (*exceptionHandlers[16])() = {
        Exception0,
        Exception1,
        Exception2,
        Exception3,
        Exception4,
        Exception5,
        Exception6,
        Exception7,
        Exception8,
        HalpDefInterruptHandler,
        Exception10,
        Exception11,
        Exception12,
        Exception13,
        Exception14,
        HalpDefInterruptHandler,
    };

    for (i = 0; i < 16; i++)
    {
        PKINTERRUPT exception;

        status = IoCreateInterrupt(
            &exception,
            i,
            exceptionHandlers[i],
            cpuNumber,
            /* TODO: Select a more appropriate IRQL for error handlers */
            SYNCH_LEVEL,
            (i == 1) || (i == 3) || (i == 4),
            NULL);

        HalpInitLegacyInterruptHandlerStub(exception, (ULONG_PTR)exceptionHandlers[i]);

        if (!NT_SUCCESS(status))
        {
            return status;
        }

        KeConnectInterrupt(exception);
    }

    KiCreateInterrupt(&clockInterrupt);
    clockInterrupt->CpuNumber = KeGetCurrentProcessorId();
    clockInterrupt->InterruptIrql = DISPATCH_LEVEL;
    clockInterrupt->Connected = FALSE;
    KeInitializeSpinLock(&clockInterrupt->Lock);
    clockInterrupt->Trap = FALSE;
    clockInterrupt->Vector = CLOCK_VECTOR;
    clockInterrupt->pfnServiceRoutine = NULL;
    clockInterrupt->pfnFullCtxRoutine = PspScheduleThread;
    clockInterrupt->pfnGetRequiresFullCtx = GetRequiresFullCtxAlways;
    clockInterrupt->SendEOI = TRUE;
    clockInterrupt->IoApicVector = -1;
    clockInterrupt->pfnSetMask = ApicSetClockMask;
    HalpInitInterruptHandlerStub(clockInterrupt, (ULONG_PTR)IrqHandler);
    KeGetPcr()->ClockInterrupt = clockInterrupt;
    KeConnectInterrupt(clockInterrupt);

    status = IoCreateInterrupt(
        &stopInterrupt,
        STOP_IPI_VECTOR,
        KeStop,
        KeGetCurrentProcessorId(),
        IPI_LEVEL,
        FALSE,
        KeStopIsr);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    if (!KeConnectInterrupt(stopInterrupt))
    {
        ASSERT(FALSE);
    }

    return STATUS_SUCCESS;
}


NTSTATUS
NTAPI
IoCreateInterrupt(
    PKINTERRUPT* pOutInterrupt,
    UCHAR Vector,
    PVOID Handler,
    ULONG CpuNumber,
    KIRQL Irql,
    BOOL  Trap,
    KSERVICE_ROUTINE Routine)
{
    PKINTERRUPT interrupt;
    NTSTATUS status;
    status = KiCreateInterrupt(&interrupt);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    InitializeInterrupt(
        interrupt,
        Vector,
        Handler,
        CpuNumber,
        Irql,
        Routine);

    interrupt->Trap = Trap;
    *pOutInterrupt = interrupt;

    return STATUS_SUCCESS;
}

/**
 * @brief
 * Implemented referencing AMD64 interrupt.c from ReactOS.
 * https://github.com/reactos/reactos/blob/master/ntoskrnl/ke/amd64/interrupt.c
 */
BOOLEAN
NTAPI
KeConnectInterrupt(
    PKINTERRUPT Interrupt)
{
    KIRQL irql;
    PKPCR pcr;

    ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
    {
        KeSetSystemAffinityThread(1ULL << Interrupt->CpuNumber);
    }

    irql = KiAcquireDispatcherLock();
    pcr = KeGetPcr();

    if (Interrupt->Connected == FALSE)
    {
        HalBindInterrupt(
            pcr->Idt,
            Interrupt->Vector,
            Interrupt->Handler,
            FALSE,
            Interrupt->Trap);
            
        /* Add the interrupt to the CPU interrupt list and the interrupt map. */
        InsertTailList(&pcr->InterruptListHead, &Interrupt->CpuListEntry);

        Interrupt->Connected = TRUE;
    }

    KiReleaseDispatcherLock(irql);
    if (irql < DISPATCH_LEVEL)
    {
        KeRevertToUserAffinityThread();
    }
    return Interrupt->Connected;
}

BOOLEAN
NTAPI
KeDisconnectInterrupt(
    PKINTERRUPT Interrupt)
{
    /* TODO */
    ASSERT(FALSE);
    return Interrupt->Connected;
}

static
VOID
HalHandleApc(
    KIRQL returningToIrql)
{
    KIRQL lockIrql = KiAcquireDispatcherLock();
    PKPCR pcr = KeGetPcr();

    if (pcr->Prcb->CurrentThread->ApcState.KernelApcPending ||
        pcr->Prcb->CurrentThread->ApcState.UserApcPending)
    {
        KeDeliverApcs(
            PsGetProcessorModeFromTrapFrame(
            pcr->Prcb->CurrentThread->KernelStackPointer));
    }
    KiReleaseDispatcherLock(lockIrql);
}

static
KIRQL
KiApplyIrql(
    KIRQL irql)
{
    KIRQL currentIrql = KeGetCurrentIrql();

    if (irql > currentIrql)
    {
        KfRaiseIrql(irql);
        return currentIrql;
    }
    else
    {
        KfLowerIrql(irql);
        return currentIrql;
    }
}

static
KIRQL
KiApplyInterruptIrql(
    PKINTERRUPT Interrupt)
{
    return KiApplyIrql(Interrupt->InterruptIrql);
}

VOID
NTAPI
HalMockupInterrupt(PKINTERRUPT Interrupt)
{
    ULONG_PTR tpr = HalGetTpr();

    /* Create a copy of the interrupt object, so the EOI is not sent */
    KINTERRUPT tmpCopy;
    tmpCopy = *Interrupt;
    tmpCopy.SendEOI = FALSE;

    if (tpr < Interrupt->InterruptIrql)
    {
        HalSetTpr(Interrupt->InterruptIrql);
    }
    HalpMockupInterruptHandler((ULONG_PTR)tmpCopy.Handler);
    HalSetTpr(tpr);
}

BOOLEAN
NTAPI
HalGenericInterruptHandlerEntry(
    PKINTERRUPT Interrupt)
{
    KIRQL irql;
    irql = KiAcquireDispatcherLock();
    KeGetCurrentThread()->ThreadIrql = irql;
    /* If this interrupt has no function defined for
     * requesting full thread context, or such function returns FALSE. */
    if (Interrupt->pfnGetRequiresFullCtx == NULL ||
        Interrupt->pfnGetRequiresFullCtx(Interrupt) == FALSE)
    {
        /* No IRQL change, so IPIs can work. (IPI_LEVEL > SYNCH_LEVEL) */
        KiReleaseDispatcherLock(KeGetCurrentIrql());
        KiApplyInterruptIrql(Interrupt);

        if (Interrupt->SendEOI != FALSE)
        {
            ApicSendEoi();
        }
        ASSERT(Interrupt->pfnServiceRoutine != NULL);
        Interrupt->pfnServiceRoutine(Interrupt, Interrupt->ServiceCtx);
        HalDisableInterrupts();
        KeLowerIrql(irql);
        return FALSE;
    }
    /* Full thread context required. */
    else
    {
        HalDisableInterrupts();
        KiReleaseDispatcherLock(irql);
        return TRUE;
    }
}

ULONG_PTR
NTAPI
HalFullCtxInterruptHandlerEntry(
    PKINTERRUPT Interrupt,
    PKTASK_STATE FullCtx)
{
    ULONG_PTR result;
    KIRQL irql;

    irql = KfRaiseIrql(Interrupt->InterruptIrql);
    if (Interrupt->SendEOI != 0)
    {
        ApicSendEoi();
    }

    ASSERT(Interrupt->pfnFullCtxRoutine != NULL);
    result = Interrupt->pfnFullCtxRoutine(Interrupt, FullCtx);

    HalDisableInterrupts();
    KiApplyIrql(KeGetCurrentThread()->ThreadIrql);
    return result;
}

VOID
NTAPI
KiSendIpiToSingleCore(
    ULONG_PTR ProcessorNumber,
    BYTE Vector)
{
    ASSERT(ProcessorNumber <= 0xFF);
    ApicSendIpi((BYTE)ProcessorNumber, 0, 0, Vector);
}

VOID
NTAPI
KeSendIpi(
    KAFFINITY TargetProcessors,
    BYTE Vector)
{
    ULONG currentId = 0;
    ULONG selfId = KeGetCurrentProcessorId();

    for (currentId = 0; currentId < KeNumberOfProcessors; currentId++)
    {
        if (currentId != selfId &&
            (TargetProcessors & (1ULL << currentId)) != 0)
        {
            KiSendIpiToSingleCore(currentId, Vector);
        }
    }
}

VOID
NTAPI
KeForceClockTick()
{
    HalMockupInterrupt(KeGetPcr()->ClockInterrupt);
}

static
inline
PVOID
HalpGetHandlerFromIdtEntry(
    KIDTENTRY64 *Entry)
{
    ULONG_PTR Handler = 0;
    Handler |= Entry->Offset0to15;
    Handler |= (ULONG_PTR)Entry->Offset16to31 << 16;
    Handler |= (ULONG_PTR)Entry->Offset32to63 << 32;

    return (PVOID)Handler;
}
