#include "IDT.h"
#include <SimpleTextIo.h>
#include "device/Keyboard.h"
#include "registers.h"
#include "GDT.h"
#include <HAL/paging.h>
#include <dispatcher.h>
#include <scheduler.h>
#include <pool.h>
#include <ntdebug.h>
#include <HAL/pcr.h>
#include <HAL/X64/APIC.h>
#include <io/apc.h>

VOID KeStop();
VOID HalpMockupInterruptHandler(ULONG_PTR Handler);

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

VOID 
ExceptionHandler(UINT64 n, UINT64 errcode, UINT64 errcode2, UINT64 rip)
{
    gExceptionHandlerPtr(n, errcode, errcode2, rip);
    PrintT("ExHandler\n");
    KeStop();
}

static
inline
PVOID
HalpGetHandlerFromIdtEntry(KIDTENTRY64 *Entry)
{
    ULONG_PTR Handler = 0;
    Handler |= Entry->Offset0to15;
    Handler |= (ULONG_PTR)Entry->Offset16to31 << 16;
    Handler |= (ULONG_PTR)Entry->Offset32to63 << 32;

    return (PVOID)Handler;
}


KIDTENTRY64
NTAPI
HalpSetIdtEntry(KIDTENTRY64* Idt, UINT64 EntryNo, PVOID Handler, BOOL Usermode, BOOL Trap)
{
    KIDTENTRY64 oldEntry = Idt[EntryNo];

    Idt[EntryNo].Selector = 0x8;
    Idt[EntryNo].Zero = 0;
    Idt[EntryNo].Offset0to15 = (UINT16) (((ULONG_PTR) Handler) & UINT16_MAX);
    Idt[EntryNo].Offset16to31 = (UINT16) ((((ULONG_PTR) Handler) >> 16) & UINT16_MAX);
    Idt[EntryNo].Offset32to63 = (UINT32) ((((ULONG_PTR) Handler) >> 32) & UINT32_MAX);
    Idt[EntryNo].Type = 0x8E | (Usermode ? (0x60) : 0x00) | (Trap ? 0 : 1);
    Idt[EntryNo].Ist = 0;

    return oldEntry;
}

VOID 
HalpSetInterruptIst(KIDTENTRY64* Idt, UINT64 EntryNo, UCHAR Ist)
{
    Idt[EntryNo].Ist = Ist;
}

KIDTENTRY64* 
HalpAllocateAndInitializeIdt()
{
    DisableInterrupts();
    KIDTR64* idtr = (KIDTR64*)
        PagingAllocatePageBlockFromRange(2, PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END);
    PKIDTENTRY64 idt = (PKIDTENTRY64)((ULONG_PTR) idtr + sizeof(KIDTR64));

    idtr->Size = sizeof(KIDTENTRY64) * 256 - 1;
    idtr->Base = idt;

    for (int a = 0; a < 256; a++)
    {
        KIDTENTRY64 result;
        VOID(*handler)();
        
        handler = HalpDefInterruptHandler;
        result = HalpSetIdtEntry(idt, a, handler, FALSE, FALSE);
        ASSERT(result.Offset0to15 == 0 && result.Offset16to31 == 0 && result.Offset32to63 == 0);
    }

    HalpLoadIdt(idtr);

    return idt;
}

/* TODO: Create some sort of a KiDeleteInterrupt function. */
NTSTATUS 
NTAPI
KiCreateInterrupt(PKINTERRUPT* ppInterrupt)
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

VOID
NTAPI
HalpInitInterruptHandlerStub(
    PKINTERRUPT pInterrupt,
    ULONG_PTR ProperHandler)
{
    SIZE_T idx = 0;

    /* push rax */
    pInterrupt->Handler[idx++] = 0x50;
    /* mov rax, ProperHandler */
    pInterrupt->Handler[idx++] = 0x48;
    pInterrupt->Handler[idx++] = 0xB8;
    *((ULONG_PTR*)&pInterrupt->Handler[idx]) = ProperHandler;
    idx += sizeof(ULONG_PTR);
    /* push rax */
    pInterrupt->Handler[idx++] = 0x50;
    /* mov rax, pInterrupt */
    pInterrupt->Handler[idx++] = 0x48;
    pInterrupt->Handler[idx++] = 0xB8;
    *((PKINTERRUPT*)&pInterrupt->Handler[idx]) = pInterrupt;
    idx += sizeof(ULONG_PTR);
    /* ret */
    pInterrupt->Handler[idx++] = 0xC3;
}

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

        if (!NT_SUCCESS(status))
        {
            return status;
        }

        KeConnectInterrupt(exception);
    }

    KiCreateInterrupt(&clockInterrupt);
    clockInterrupt->CpuNumber = KeGetCurrentProcessorId();
    clockInterrupt->InterruptIrql = CLOCK_LEVEL;
    clockInterrupt->Connected = FALSE;
    KeInitializeSpinLock(&clockInterrupt->Lock);
    clockInterrupt->Trap = FALSE;
    clockInterrupt->Vector = CLOCK_VECTOR;
    clockInterrupt->pfnServiceRoutine = NULL;
    clockInterrupt->pfnFullCtxRoutine = PspScheduleThread;
    clockInterrupt->pfnGetRequiresFullCtx = GetRequiresFullCtxAlways;
    clockInterrupt->SendEOI = TRUE;
    HalpInitInterruptHandlerStub(clockInterrupt, (ULONG_PTR)IrqHandler);
    KeGetPcr()->ClockInterrupt = clockInterrupt;
    KeConnectInterrupt(clockInterrupt);

    status = IoCreateInterrupt(
        &stopInterrupt, 
        STOP_IPI_VECTOR, 
        IrqHandler, 
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

VOID
NTAPI
HalMockupInterrupt(PKINTERRUPT Interrupt)
{
    /* Create a copy of the interrupt object, so the EOI is not sent */
    KINTERRUPT tmpCopy;

    tmpCopy = *Interrupt;
    tmpCopy.SendEOI = FALSE;
    HalpMockupInterruptHandler((ULONG_PTR)tmpCopy.Handler);
}

VOID
NTAPI
KeForceClockTick()
{
    HalMockupInterrupt(KeGetPcr()->ClockInterrupt);
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
    if (KeGetCurrentIrql() == PASSIVE_LEVEL)
    {
        KeSetSystemAffinityThread(1ULL << Interrupt->CpuNumber);
    }

    irql = KiAcquireDispatcherLock();
    pcr = KeGetPcr();

    if (Interrupt->Connected == FALSE)
    {
        /* TODO: Interrupt->pfnServiceRoutine should be a second stage of interrupt,
         * handling. It should be called from an universal handler managing
         * IRQL from the KINTERRUPT structure etc. */
        HalpSetIdtEntry(
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
    if (irql == PASSIVE_LEVEL)
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
HalHandleApc(KIRQL returningToIrql)
{
    KIRQL lockIrql = KiAcquireDispatcherLock();
    PKPCR pcr = KeGetPcr();

    if (pcr->Prcb->CurrentThread->ApcState.KernelApcPending ||
        pcr->Prcb->CurrentThread->ApcState.UserApcPending)
    {
        KeDeliverApcs(
            PsGetProcessorModeFromTrapFrame(
                pcr->Prcb->CurrentThread->KernelStackPointer
            )
        );
    }
    KiReleaseDispatcherLock(lockIrql);
}

static
KIRQL
KiApplyInterruptIrql(
    PKINTERRUPT Interrupt)
{
    KIRQL iirql = Interrupt->InterruptIrql;
    KIRQL currentIrql = KeGetCurrentIrql();

    if (iirql > currentIrql)
    {
        KfRaiseIrql(iirql);
        return currentIrql;
    }
    else
    {
        KfLowerIrql(iirql);
        return currentIrql;
    }
}

BOOLEAN
NTAPI
HalGenericInterruptHandlerEntry(
    PKINTERRUPT Interrupt)
{
    KIRQL irql;
    irql = KiAcquireDispatcherLock();

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
        DisableInterrupts();
        KeLowerIrql(irql);
        return FALSE;
    }
    /* Full thread context required. */
    else
    {
        DisableInterrupts();
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

    DisableInterrupts();
    KeLowerIrql(irql);
    return result;
}

KSPIN_LOCK IpiLock = 0;

VOID
NTAPI
KiSendIpiToSingleCore(
    ULONG_PTR ProcessorNumber,
    BYTE Vector)
{
    ASSERT(IpiLock != 0);
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
    KIRQL irql;
    ULONG selfId = KeGetCurrentProcessorId();

    KeRaiseIrql(DISPATCH_LEVEL, &irql);
    KeAcquireSpinLockAtDpcLevel(&IpiLock);
    KfRaiseIrql(IPI_LEVEL);

    for (currentId = 0; currentId < KeNumberOfProcessors; currentId++)
    {
        if (currentId != selfId &&
            (TargetProcessors & (1ULL << currentId)) != 0)
        {
            KiSendIpiToSingleCore(currentId, Vector);
        }
    }

    KeReleaseSpinLockFromDpcLevel(&IpiLock);
    KeLowerIrql(irql);
}
