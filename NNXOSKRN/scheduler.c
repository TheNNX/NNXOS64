#include <scheduler.h>
#include <pool.h>
#include <paging.h>
#include <SimpleTextIo.h>
#include <rtl.h>
#include <physical_allocator.h>
#include <pcr.h>
#include <bugcheck.h>
#include <cpu.h>
#include <object.h>
#include <dispatcher.h>
#include <apc.h>
#include <ntdebug.h>
#include <mm.h>
#include <file.h>
#include <intrin.h>
#include <dpc.h>

LIST_ENTRY ProcessListHead;
LIST_ENTRY ThreadListHead;

UCHAR PspForegroundQuantum[3] = { 0x06, 0x0C, 0x12 };
UCHAR PspPrioritySeparation = 2;
UINT PspCoresInitialized = 0;

static KDPC PsThreadCleanupDpc;

const CHAR PspThrePoolTag[4] = "Thre";
const CHAR PspProcPoolTag[4] = "Proc";
static UNICODE_STRING PsProcessTypeName = RTL_CONSTANT_STRING(L"Process");
static UNICODE_STRING PsThreadTypeName = RTL_CONSTANT_STRING(L"Thread");
POBJECT_TYPE PsProcessType = NULL;
POBJECT_TYPE PsThreadType = NULL;

extern ULONG_PTR KeMaximumIncrement;
extern ULONG_PTR KiCyclesPerClockQuantum;
extern ULONG_PTR KiCyclesPerQuantum;
extern UINT KeNumberOfProcessors;

#include <scheduler_internal.h>

struct _READY_QUEUES
{
    /* A ready queue for each thread priority */
    LIST_ENTRY ThreadReadyQueues[32];
    ULONG ThreadReadyQueuesSummary;
};

typedef struct _THREAD_ON_CREATE_DATA
{
    ULONG_PTR Entrypoint;
    BOOL IsKernel;
    PEPROCESS ParentProcess;
}THREAD_ON_CREATE_DATA, * PTHREAD_ON_CREATE_DATA;

typedef struct _KPROCESSOR_READY_QUEUES
{
    struct _READY_QUEUES;
    PEPROCESS IdleProcess;
    PETHREAD IdleThread;
}KCORE_SCHEDULER_DATA, *PKCORE_SCHEDULER_DATA;

typedef struct _KSHARED_READY_QUEUE
{
    struct _READY_QUEUES;
}KSHARED_READY_QUEUE, *PKSHARED_READY_QUEUE;

PKCORE_SCHEDULER_DATA   CoresSchedulerData = (PKCORE_SCHEDULER_DATA)NULL;
KSHARED_READY_QUEUE     PspSharedReadyQueue = { 0 };

/* Clear the summary bit for this priority if there are none entries left */
inline 
VOID 
ClearSummaryBitIfNeccessary(LIST_ENTRY* ThreadReadyQueues,
                            PULONG Summary,
                            UCHAR Priority)
{
    if (IsListEmpty(&ThreadReadyQueues[Priority]))
    {
        _bittestandreset(Summary, Priority);
    }
}

inline 
VOID
SetSummaryBitIfNeccessary(LIST_ENTRY* ThreadReadyQueues, 
                          PULONG Summary, 
                          UCHAR Priority)
{
    if (!IsListEmpty(&ThreadReadyQueues[Priority]))
    {
        _bittestandset(Summary, Priority);
    }
}

static
VOID
ThreadCleanupRoutine(PKDPC Dpc,
                     PVOID Context,
                     PVOID Thread,
                     PVOID SystemArgument2)
{
    ObDereferenceObject(Thread);
}

NTSTATUS 
NTAPI
PspInitializeScheduler()
{
    INT i;
    KIRQL irql;
    NTSTATUS status;

    status = ObCreateType(&PsProcessType,
                          &PsProcessTypeName,
                          sizeof(EPROCESS));
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = ObCreateType(&PsThreadType,
                          &PsThreadTypeName,
                          sizeof(ETHREAD));
    if (!NT_SUCCESS(status))
    {
        ObDereferenceObject(PsProcessType);
        return status;
    }

    PsProcessType->OnCreate = PspProcessOnCreate;
    PsProcessType->OnDelete = PspProcessOnDelete;
    PsThreadType->OnCreate = PspThreadOnCreate;
    PsThreadType->OnDelete = PspThreadOnDelete;

    KeInitializeDpc(&PsThreadCleanupDpc, ThreadCleanupRoutine, NULL);

    irql = KiAcquireDispatcherLock();

    if (CoresSchedulerData == NULL)
    {
        InitializeListHead(&ThreadListHead);
        InitializeListHead(&ProcessListHead);

        CoresSchedulerData = (PKCORE_SCHEDULER_DATA)
            PagingAllocatePageBlockFromRange(
                (KeNumberOfProcessors * sizeof(KCORE_SCHEDULER_DATA) 
                    + PAGE_SIZE - 1) / PAGE_SIZE,
                PAGING_KERNEL_SPACE,
                PAGING_KERNEL_SPACE_END);
            
        if (CoresSchedulerData != NULL)
        {
            for (i = 0; i < 32; i++)
            {
                InitializeListHead(&PspSharedReadyQueue.ThreadReadyQueues[i]);
            }
        }
    }
    KiReleaseDispatcherLock(irql);
    return 
        (CoresSchedulerData == (PKCORE_SCHEDULER_DATA)NULL) ? 
            STATUS_NO_MEMORY : 
            STATUS_SUCCESS;
}

PKTHREAD 
NTAPI
PspSelectNextReadyThread(
    UCHAR CoreNumber)
{
    INT priority;
    PKTHREAD result;
    PLIST_ENTRY dequeuedEntry;
    PKCORE_SCHEDULER_DATA coreOwnData = &CoresSchedulerData[CoreNumber];

    PspManageSharedReadyQueue(CoreNumber);
    result = &coreOwnData->IdleThread->Tcb;

    /* Start with the highest priority. */
    for (priority = 31; priority >= 0; priority--)
    {
        if (_bittest(&coreOwnData->ThreadReadyQueuesSummary, priority))
        {
            dequeuedEntry = 
                RemoveHeadList(&coreOwnData->ThreadReadyQueues[priority]);

            result = (PKTHREAD)
                ((ULONG_PTR)dequeuedEntry - 
                    FIELD_OFFSET(KTHREAD, ReadyQueueEntry));

            ClearSummaryBitIfNeccessary(
                coreOwnData->ThreadReadyQueues, 
                &coreOwnData->ThreadReadyQueuesSummary, 
                priority);
            break;
        }
    }

    /* If no process was found on queue(s), return the idle thread. */
    return result;
}

NTSTATUS 
NTAPI
PspCreateIdleProcessForCore(
    PEPROCESS* outIdleProcess, 
    PETHREAD* outIdleThread, 
    UINT8 coreNumber) 
{
    NTSTATUS status;
    PEPROCESS pIdleProcess;
    PETHREAD pIdleThread;
    THREAD_ON_CREATE_DATA threadCreationData;

    /* Since DispatcherLock is already held, we cannot call the ObjectManager 
     * to create the idle thread. This isn't a problem, because the idle thread
     * cannot be dereferenced or deleted, and as such can be manually created, 
     * without deadlock due to trying to acquire a lock already held bu this
     * core. */

    /* Allocate memory for the process structure. */
    pIdleProcess = ExAllocatePool(NonPagedPool, sizeof(*pIdleProcess));
    if (pIdleProcess == NULL)
        return STATUS_NO_MEMORY;

    /* Initialize the process structure. */
    status = PspProcessOnCreateNoDispatcher((PVOID)pIdleProcess, NULL);
    if (!NT_SUCCESS(status))
    {
        ExFreePool(pIdleProcess);
        return status;
    }
    pIdleProcess->Pcb.AffinityMask = (1ULL << (ULONG_PTR)coreNumber);

    /* Allocate memory for the thread structure. */
    pIdleThread = ExAllocatePool(NonPagedPool, sizeof(*pIdleThread));
    if (pIdleThread == NULL)
    {
        ExFreePool(pIdleProcess);
        return STATUS_NO_MEMORY;
    }

    /* Initialize the thread creation data structure, 
     * which is neccessary to initialize the thread. */
    threadCreationData.Entrypoint = (ULONG_PTR)PspIdleThreadProcedure;
    threadCreationData.IsKernel = TRUE;
    threadCreationData.ParentProcess = pIdleProcess;

    /* Initialize the thread structure. */
    status = PspThreadOnCreateNoDispatcher(
        (PVOID)pIdleThread,
        &threadCreationData);

    if (!NT_SUCCESS(status))
    {
        ExFreePool(pIdleProcess);
        ExFreePool(pIdleThread);
        return status;
    }
    pIdleThread->Tcb.ThreadPriority = 0;

    PrintT("Core %i's idle thread %X\n", coreNumber, pIdleThread);

    /* Add the idle process and the idle thread to their respective lists. */
    InsertHeadList(&ProcessListHead, &pIdleProcess->Pcb.ProcessListEntry);
    InsertHeadList(&ThreadListHead, &pIdleThread->Tcb.ThreadListEntry);

    *outIdleProcess = pIdleProcess;
    *outIdleThread = pIdleThread;

    return STATUS_SUCCESS;
}

VOID 
NTAPI
PspInitializeCoreSchedulerData(
    UINT8 CoreNumber)
{
    INT i;
    PKCORE_SCHEDULER_DATA thiscoreSchedulerData =
        &CoresSchedulerData[CoreNumber];

    NTSTATUS status;

    thiscoreSchedulerData->ThreadReadyQueuesSummary = 0;

    for (i = 0; i < 32; i++)
    {
        InitializeListHead(&thiscoreSchedulerData->ThreadReadyQueues[i]);
    }

    status = PspCreateIdleProcessForCore(
        &thiscoreSchedulerData->IdleProcess, 
        &thiscoreSchedulerData->IdleThread, 
        CoreNumber);
    
    PrintT("PspCreateIdleProcessForCore status: %X\n", status);
    if (status != STATUS_SUCCESS)
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, status, 0, 0, 0);
}

static
NTSTATUS
PsConnectNotifyIpi()
{
    PKINTERRUPT notifyInterrupt;
    NTSTATUS status;

    status = KiCreateInterrupt(&notifyInterrupt);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    
    notifyInterrupt->CpuNumber = KeGetCurrentProcessorId();
    notifyInterrupt->InterruptIrql = CLOCK_LEVEL;
    notifyInterrupt->Connected = FALSE;
    KeInitializeSpinLock(&notifyInterrupt->Lock);
    notifyInterrupt->Trap = FALSE;
    notifyInterrupt->Vector = SCHEDULER_NOTIFY_IPI_VECTOR;
    notifyInterrupt->pfnServiceRoutine = NULL;
    notifyInterrupt->pfnFullCtxRoutine = PspScheduleThread;
    notifyInterrupt->pfnGetRequiresFullCtx = GetRequiresFullCtxAlways;
    notifyInterrupt->SendEOI = TRUE;
    notifyInterrupt->IoApicVector = -1;
    notifyInterrupt->pfnSetMask = NULL;
    HalpInitInterruptHandlerStub(notifyInterrupt, (ULONG_PTR)IrqHandler);
    
    if (!KeConnectInterrupt(notifyInterrupt))
    {
        return STATUS_INTERNAL_ERROR;
    }

    return STATUS_SUCCESS;
}

#pragma warning(push)
NTSTATUS 
NTAPI
PspInitializeCore(
    UINT8 CoreNumber)
{
    KIRQL irql;
    PKPCR pPcr;
    PKPRCB pPrcb;
    NTSTATUS status = STATUS_SUCCESS;
    PKTASK_STATE pTaskState;
    PKTHREAD pDummyThread;

    /* check if this is the first core to be initialized */
    /* if so, initialize the scheduler first */
    if (CoresSchedulerData == NULL)
    {
        status = PspInitializeScheduler();

        if (!NT_SUCCESS(status))
        {
            return status;
        }
    }

    KeRaiseIrql(DISPATCH_LEVEL, &irql);
    PspInitializeCoreSchedulerData(CoreNumber);
    KeLowerIrql(irql);

    irql = KiAcquireDispatcherLock();

    pPcr = KeGetPcr();
    pPrcb = pPcr->Prcb;
    KeAcquireSpinLockAtDpcLevel(&pPrcb->Lock);

    /* Copy the idle thread info into the PRCB. */
    ASSERT(CoresSchedulerData != NULL);
    pPrcb->IdleThread = &CoresSchedulerData[CoreNumber].IdleThread->Tcb;
    pPrcb->NextThread = pPrcb->IdleThread;
    pPrcb->CurrentThread = pPrcb->IdleThread;

    pPrcb->DpcStack = PspCreateKernelStack(1);

    pDummyThread = pPrcb->DummyThread;
    pPrcb->DummyThread = NULL;

    pPcr->CyclesLeft = (LONG_PTR)KiCyclesPerQuantum * 3;
    KeReleaseSpinLockFromDpcLevel(&pPrcb->Lock);

    PspCoresInitialized++;
    
    MmApplyAddressSpace(&pPrcb->IdleThread->Process->AddressSpace);

    pPrcb->IdleThread->ThreadState = THREAD_STATE_RUNNING;
    KiReleaseDispatcherLock(irql);

    /* Free the dummy thread used for system initialization. */
    ExFreePool(CONTAINING_RECORD(pDummyThread->Process, EPROCESS, Pcb));
    ExFreePool(CONTAINING_RECORD(pDummyThread, ETHREAD, Tcb));

    PsConnectNotifyIpi();

    if (PspCoresInitialized == KeNumberOfProcessors)
    {
        NTSTATUS status;
        HANDLE hProcess;

        UNICODE_STRING t = RTL_CONSTANT_STRING(L"NTDLL.DLL");
        PrintT("Starting NTDLL.DLL\n");

        status = NnxStartUserProcess(&t, &hProcess, 10);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        PrintT("hProcess: %X\n");
    }

    pTaskState = pPrcb->IdleThread->KernelStackPointer;
    
    MmApplyAddressSpace(&pPrcb->IdleThread->Process->AddressSpace);
    
    HalDisableInterrupts();
    KeLowerIrql(0);
    
    PrintT("Core %i scheduler initialized %i\n", 
           KeGetCurrentProcessorId(), 
           KeGetCurrentIrql());


    HalpApplyTaskState(pTaskState);

    return STATUS_SUCCESS;
}

#pragma warning(pop)

static
VOID
PspCallDpc(PKDPC Dpc, 
           PKTASK_STATE ResumeStack)
{
    KfRaiseIrql(DPC_LEVEL);

    Dpc->Routine(Dpc, Dpc->Context, Dpc->SystemArgument1, Dpc->SystemArgument2);

    KfRaiseIrql(HIGH_LEVEL);
    KeGetCurrentThread()->SwitchStackPointer = ResumeStack;

    KfLowerIrql(PASSIVE_LEVEL);
    KeForceClockTick();
}

static
VOID
PspCheckDpcQueue(PKTHREAD Thread)
{
    PKDPC_DATA dpcData;
    ULONG_PTR dpcStack;
    PVOID resumeStack;
    PKTASK_STATE taskState;
    PLIST_ENTRY entry;
    PKDPC dpc;
    KIRQL irql;

    dpcStack = (ULONG_PTR)KeGetPcr()->Prcb->DpcStack;

    /* We're currently in a DPC handler, trying to set it up again would trash
       the DPC stack. */
    if (KeGetPcr()->Prcb->DpcEnding)
    {
        KeGetPcr()->Prcb->DpcInProgress = FALSE;
        KeGetPcr()->Prcb->DpcEnding = FALSE;
        return;
    }

    if (KeGetPcr()->Prcb->DpcInProgress)
    {
        return;
    }

    ASSERT(!((ULONG_PTR) & dpc >= dpcStack - 4096 && (ULONG_PTR)&dpc < dpcStack));

    if (Thread->ThreadIrql > DPC_LEVEL)
    {
        return;
    }
    
    irql = KfRaiseIrql(HIGH_LEVEL);

    dpcData = &KeGetPcr()->Prcb->DpcData;
    KiAcquireSpinLock(&dpcData->DpcLock);

    if (dpcData->DpcQueueDepth == 0)
    {
        KiReleaseSpinLock(&dpcData->DpcLock);
        KeLowerIrql(irql);
        return;
    }

    KeGetPcr()->Prcb->DpcInProgress = TRUE;

    dpcData->DpcQueueDepth--;
    entry = RemoveHeadList(&dpcData->DpcListHead);

    dpc = CONTAINING_RECORD(entry, KDPC, Entry);
    dpc->Inserted = FALSE;

    resumeStack = Thread->KernelStackPointer;
    dpcStack -= 32;

    taskState = (PKTASK_STATE)(dpcStack - sizeof(KTASK_STATE) - 1000);

    PspSetupThreadState(taskState, TRUE, (ULONG_PTR)PspCallDpc, dpcStack);
    taskState->R15 = 0x1515'1515'1515'1515;

    Thread->KernelStackPointer = (PVOID)taskState;
    ASSERT((ULONG_PTR)Thread->KernelStackPointer > 0xFFFFFF);

    PspSetUsercallParameter(Thread, 0, (ULONG_PTR)dpc);
    PspSetUsercallParameter(Thread, 1, (ULONG_PTR)resumeStack);

    KiReleaseSpinLock(&dpcData->DpcLock);
    KeLowerIrql(irql);
}

ULONG_PTR 
NTAPI
PspScheduleThread(PKINTERRUPT InterruptSource, 
                  PKTASK_STATE Stack)
{
    PKPCR pcr;
    KIRQL irql;

    PKSPIN_LOCK originalRunningProcessSpinlock;
    PKTHREAD nextThread;
    PKTHREAD originalRunningThread;
    PKPROCESS originalRunningProcess;
    UCHAR origRunThrdPriority;

    irql = KiAcquireDispatcherLock();
    pcr = KeGetPcr();
    
    if (pcr->ClockInterrupt == InterruptSource)
    {
        KiClockTick();
    }
    
    KeAcquireSpinLockAtDpcLevel(&pcr->Prcb->Lock);

    originalRunningThread = pcr->Prcb->CurrentThread;
    
    if (originalRunningThread == NULL)
    {
        KeBugCheckEx(
            WORKER_THREAD_TEST_CONDITION, 
            (ULONG_PTR)originalRunningThread, 
            0, 0, 0);
    }

    /* The thread has IRQL too high for the scheduler, the clock interrupt
     * should only update the clock state. */
    if (originalRunningThread->ThreadIrql >= DISPATCH_LEVEL)
    {
        KeReleaseSpinLockFromDpcLevel(&pcr->Prcb->Lock);
        KiReleaseDispatcherLock(irql);
        return (ULONG_PTR)NULL;
    }

    originalRunningThread->KernelStackPointer = (PVOID)Stack;
    ASSERT((ULONG_PTR)originalRunningThread->KernelStackPointer > 0xFFFFFF);

    if (originalRunningThread->SwitchStackPointer)
    {
        originalRunningThread->KernelStackPointer = originalRunningThread->SwitchStackPointer;
        originalRunningThread->LastSwitchStackPointer = originalRunningThread->KernelStackPointer;
        originalRunningThread->SwitchStackPointer = NULL;

        pcr->Prcb->DpcEnding = TRUE;

        ASSERT((ULONG_PTR)originalRunningThread->KernelStackPointer > 0xFFFFFF);
    }
    else
    {
        originalRunningThread->LastSwitchStackPointer = 0;
    }

    if (originalRunningThread->ThreadState != THREAD_STATE_RUNNING)
    {
        pcr->CyclesLeft = 0;
    }

    originalRunningProcess = originalRunningThread->Process;
    if (originalRunningProcess == NULL)
    {
        KeBugCheckEx(
            WORKER_THREAD_TEST_CONDITION, 
            (ULONG_PTR)originalRunningThread, 
            0, 0, 0);
    }
    originalRunningProcessSpinlock = &originalRunningProcess->ProcessLock;
    KeAcquireSpinLockAtDpcLevel(originalRunningProcessSpinlock);

    origRunThrdPriority = 
        originalRunningThread->ThreadPriority + 
        originalRunningProcess->BasePriority;

    /* If no next thread has been selected,
     * or the currently selected thread is not ready */
    if (pcr->Prcb->IdleThread == pcr->Prcb->NextThread || 
        pcr->Prcb->NextThread->ThreadState != THREAD_STATE_READY)
    {
        /* Select a new next thread. */
        pcr->Prcb->NextThread = PspSelectNextReadyThread(pcr->Prcb->Number);
    }

    /* If the thread has already used all its CPU time, 
     * or it is terminated and should be deleted */
    if (pcr->CyclesLeft < (LONG_PTR)KiCyclesPerQuantum || 
        pcr->Prcb->CurrentThread->ThreadState == THREAD_STATE_TERMINATED)
    {
        /* If no next thread was found, or the next thread can't preempt 
         * the current one and the current thread is not waiting, 
         * reset the quantum for current thread. */
        if (((pcr->Prcb->NextThread == pcr->Prcb->IdleThread && 
              pcr->Prcb->CurrentThread != pcr->Prcb->IdleThread) ||
            (origRunThrdPriority > pcr->Prcb->NextThread->ThreadPriority + 
             pcr->Prcb->NextThread->Process->BasePriority)) &&
            pcr->Prcb->CurrentThread->ThreadState == THREAD_STATE_RUNNING)
        {
            pcr->CyclesLeft = 
                originalRunningProcess->QuantumReset * KiCyclesPerQuantum;
        }
        else
        {
            /* Switch to next thread */
            nextThread = pcr->Prcb->NextThread;
            pcr->CyclesLeft = 
                nextThread->Process->QuantumReset * KiCyclesPerQuantum;

            /* if thread was found */
            if (nextThread != originalRunningThread)
            {
                if (nextThread->CustomAddressSpace != NULL)
                {
                    MmApplyAddressSpace(nextThread->CustomAddressSpace);
                }
                else if (pcr->Prcb->CurrentThread->Process != 
                    pcr->Prcb->NextThread->Process)
                {
                    MmApplyAddressSpace(&nextThread->Process->AddressSpace);
                }
            }

            nextThread->ThreadState = THREAD_STATE_RUNNING;

            /* select thread before setting the original one to ready */
            pcr->Prcb->NextThread = PspSelectNextReadyThread(pcr->Prcb->Number);
            pcr->Prcb->CurrentThread = nextThread;

            /* The thread could have volountarily given up control, 
             * it could be waiting - then its state shouldn't be changed. */
            if (originalRunningThread->ThreadState == THREAD_STATE_RUNNING)
            {
                PspInsertIntoSharedQueue(originalRunningThread);
                originalRunningThread->ThreadState = THREAD_STATE_READY;
            }
            else if (originalRunningThread->ThreadState == 
                     THREAD_STATE_TERMINATED)
            {
                PrintT("Dereferencing terminated thread %X\n", originalRunningThread);
                KeInsertQueueDpc(&PsThreadCleanupDpc, originalRunningThread, NULL);
            }
        }
    }
    else
    {
        pcr->CyclesLeft -= KiCyclesPerQuantum;

        if (pcr->CyclesLeft < 0)
        {
            pcr->CyclesLeft = 0;
        }
    }

    KeReleaseSpinLockFromDpcLevel(originalRunningProcessSpinlock);
    KeReleaseSpinLockFromDpcLevel(&pcr->Prcb->Lock);

    PspCheckDpcQueue(pcr->Prcb->CurrentThread);

    KiReleaseDispatcherLock(irql);

    PVOID sane = (PVOID)0x1000000;

    ASSERT((ULONG_PTR)pcr > (ULONG_PTR)sane);
    ASSERT((ULONG_PTR)pcr->Prcb > (ULONG_PTR)sane);
    ASSERT((ULONG_PTR)pcr->Prcb->CurrentThread > (ULONG_PTR)sane);
    ASSERT((ULONG_PTR)pcr->Prcb->CurrentThread->KernelStackPointer > (ULONG_PTR)sane);
    return (ULONG_PTR)pcr->Prcb->CurrentThread->KernelStackPointer;
}

PKTHREAD 
NTAPI
PspGetCurrentThread()
{
    PKPCR pcr = KeGetPcr();
    return pcr->Prcb->CurrentThread;
}

PKTHREAD 
NTAPI
KeGetCurrentThread()
{
    return PspGetCurrentThread();
}

PVOID 
NTAPI
PspCreateKernelStack(SIZE_T nPages)
{
    ULONG_PTR blockAllocation, currentMappingGuard1, currentMappingGuard2;
    NTSTATUS status;

    /* Allocate two more pages for the page guard. */
    blockAllocation = PagingAllocatePageBlockFromRange(
        nPages + 2, 
        PAGING_KERNEL_SPACE, 
        PAGING_KERNEL_SPACE_END);

    /* Get the current mappings. */
    currentMappingGuard1 = 
        PagingGetTableMapping(blockAllocation)
        & PAGE_ADDRESS_MASK;
    currentMappingGuard2 = 
        PagingGetTableMapping(blockAllocation + PAGE_SIZE * (nPages + 1))
        & PAGE_ADDRESS_MASK;

    /* FIXME:
     * Currently, this marks as zero address, non present, non-zero flags. 
     * Page fault handler doesn't recognize pagefile index 0 as valid,
     * and PagingFindFreePages doesn't consider pages with non-zero flags
     * as free. */
    status = PagingMapPage(
        blockAllocation, 
        0, 
        PAGE_WRITE);

    if (!NT_SUCCESS(status))
    {
        PrintT("[%s:%i] Not success\n", __FILE__, __LINE__);
        return (PVOID)NULL;
    }
    
    status = PagingMapPage(
        blockAllocation + PAGE_SIZE * (nPages + 1), 
        0, 
        PAGE_WRITE);

    if (!NT_SUCCESS(status))
    {
        PrintT("[%s:%i] Not success\n", __FILE__, __LINE__);
        return (PVOID)NULL;
    }

    /* Free the PFNs of the guards. */
    MmFreePhysicalAddress(currentMappingGuard1);
    MmFreePhysicalAddress(currentMappingGuard2);
    
    return (PVOID)(blockAllocation + (nPages + 1) * PAGE_SIZE);
}

VOID
NTAPI
PspFreeKernelStack(
    PVOID OriginalStackLocation,
    SIZE_T nPages
)
{
    /* TODO */
}

VOID 
NTAPI
PspSetupThreadState(PKTASK_STATE pThreadState,
                    BOOL IsKernel,
                    ULONG_PTR Entrypoint,
                    ULONG_PTR Userstack)
{
    UINT16 code0, code3, data0, data3;
    LPKGDTENTRY64 gdt;

    gdt = KeGetPcr()->Gdt;

    code0 = HalpGdtFindEntry(gdt, 7, TRUE, FALSE);
    code3 = HalpGdtFindEntry(gdt, 7, TRUE, TRUE);

    data0 = HalpGdtFindEntry(gdt, 7, FALSE, FALSE);
    data3 = HalpGdtFindEntry(gdt, 7, FALSE, TRUE);

    RtlZeroMemory(pThreadState, sizeof(*pThreadState));
    pThreadState->Rip = (UINT64)Entrypoint;
    pThreadState->Cs = IsKernel ? code0 : code3;
    pThreadState->Ds = IsKernel ? data0 : data3;
    pThreadState->Es = IsKernel ? data0 : data3;
    pThreadState->Fs = IsKernel ? data0 : data3;
    pThreadState->Gs = IsKernel ? data0 : data3;
    pThreadState->Ss = IsKernel ? data0 : data3;
    pThreadState->Rflags = 0x00000286;
    pThreadState->Rsp = Userstack;
    pThreadState->R14 = 0xABCDEF;
}

NTSTATUS 
NTAPI
PspCreateProcessInternal(
    PEPROCESS* ppProcess, 
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES pProcessObjAttributes)
{
    NTSTATUS status;
    PEPROCESS pProcess;
    OBJECT_ATTRIBUTES processObjAttributes;

    /* Create the process object. */

    if (pProcessObjAttributes == NULL)
    {
        InitializeObjectAttributes(
            &processObjAttributes,
            NULL,
            OBJ_KERNEL_HANDLE,
            NULL,
            NULL);
        pProcessObjAttributes = &processObjAttributes;
    }
    status = ObCreateObject(
        &pProcess, 
        DesiredAccess, 
        KernelMode, 
        pProcessObjAttributes,
        PsProcessType,
        NULL);

    if (status != STATUS_SUCCESS)
        return status;

    *ppProcess = pProcess;

    return STATUS_SUCCESS;
}


NTSTATUS 
NTAPI
PspProcessOnCreate(
    PVOID selfObject, 
    PVOID createData)
{
    PEPROCESS pProcess;
    NTSTATUS status;
    KIRQL irql;

    pProcess = (PEPROCESS)selfObject;

    /* Acquire the dispatcher lock. */
    irql = KiAcquireDispatcherLock();

    /* Initialize the dispatcher header. */
    InitializeDispatcherHeader(&pProcess->Pcb.Header, ProcessObject);

    /* Initialize the process structure. */
    status = PspProcessOnCreateNoDispatcher(selfObject, createData);
    /* Add the process to the process list. */
    InsertTailList(&ProcessListHead, &pProcess->Pcb.ProcessListEntry);

    /* Release the dispatcher lock. */
    KiReleaseDispatcherLock(irql);

    return status;
}

NTSTATUS
NTAPI
PspProcessOnCreateNoDispatcher(
    PVOID SelfObject,
    PVOID CreateData)
{
    PEPROCESS pProcess = (PEPROCESS)SelfObject;

    pProcess->Initialized = FALSE;
    KeInitializeSpinLock(&pProcess->Pcb.ProcessLock);
    KeInitializeSpinLock(&pProcess->Pcb.ProcessLock);
    InitializeListHead(&pProcess->Pcb.ThreadListHead);
    pProcess->Pcb.BasePriority = 0;
    pProcess->Pcb.AffinityMask = KAFFINITY_ALL;
    pProcess->Pcb.NumberOfThreads = 0;
    MmCreateAddressSpace(&pProcess->Pcb.AddressSpace);
    pProcess->Pcb.QuantumReset = 6;
    InitializeListHead(&pProcess->Pcb.HandleDatabaseHead);
    InitializeListHead(&pProcess->Pcb.LdrModulesHead);

    return STATUS_SUCCESS;
}

/**
 * @brief Allocates memory for a new thread, adds it to the scheduler's thread 
 * list and parent process' child thread list.
 * @param ppThread pointer to a pointer to PETHREAD, value it's pointing to will
 * be set to result of allocation after this function.
 * @param pParentProcess pointer to EPROCESS structure of the parent process for
 * this thread.
 * @param IsKernel if true, thread is created in kernel mode.
 * @param EntryPoint entrypoint function for the thread, caller is responsible
 * for making any neccessary changes in the parent process' address space.
 * @return STATUS_SUCCESS on success
*/
NTSTATUS 
NTAPI
PspCreateThreadInternal(
    PETHREAD* ppThread, 
    PEPROCESS pParentProcess, 
    BOOL IsKernel, 
    ULONG_PTR EntryPoint)
{
    NTSTATUS status;
    PETHREAD pThread;
    OBJECT_ATTRIBUTES threadObjAttributes;
    THREAD_ON_CREATE_DATA data;

    InitializeObjectAttributes(
        &threadObjAttributes,
        NULL,
        OBJ_KERNEL_HANDLE,
        NULL,
        NULL);

    data.Entrypoint = EntryPoint;
    data.ParentProcess = pParentProcess;
    data.IsKernel = IsKernel;

    status = ObCreateObject(
        &pThread,
        0,
        KernelMode,
        &threadObjAttributes,
        PsThreadType,
        &data);

    if (status != STATUS_SUCCESS)
        return status;

    *ppThread = pThread;

    return STATUS_SUCCESS;
}

/**
 * @brief This function is called by the object manager when a new thread
 * is being created. It initializes the dispatcher header of the thread,
 * the ETHREAD structure, adds the thread to parent process' child
 * list and adds the thread to the system wide thread list.
 * @param SelfObject pointer to Object Manager allocated object
 * @param CreateData pointer to THREAD_CREATE_DATA structure,
 * which holds the entrypoint and parent process pointer for example. 
 */
NTSTATUS 
NTAPI
PspThreadOnCreate(
    PVOID SelfObject,
    PVOID CreateData)
{
    KIRQL irql;
    NTSTATUS status;
    PETHREAD pThread;
    
    pThread = (PETHREAD)SelfObject;

    /* Acquire the dispatcher lock. */
    irql = KiAcquireDispatcherLock();

    /* Initialize the dispatcher header. */
    InitializeDispatcherHeader(&pThread->Tcb.Header, ThreadObject);

    /* Initialize the thread structure. */
    status = PspThreadOnCreateNoDispatcher(SelfObject, CreateData);

    /* Initialize the parts of the thread structure 
     * that are "protected" by the dispatcher lock. */
    InsertTailList(&ThreadListHead, &pThread->Tcb.ThreadListEntry);
    KeInitializeApcState(&pThread->Tcb.ApcState);
    KeInitializeApcState(&pThread->Tcb.SavedApcState);
    pThread->Tcb.ThreadState = THREAD_STATE_READY;

    /* Release the lock and return. */
    KiReleaseDispatcherLock(irql);
    ObReferenceObject(pThread->Process);
    return status;
}

VOID
NTAPI
PspOnThreadTimeout(
    PKTIMEOUT_ENTRY pTimeout)
{
    PKTHREAD pThread = CONTAINING_RECORD(pTimeout, KTHREAD, TimeoutEntry);
   
    ASSERT(KeGetCurrentIrql() >= DISPATCH_LEVEL);
    KeAcquireSpinLockAtDpcLevel(&pThread->ThreadLock);
    ASSERT(pThread->ThreadState == THREAD_STATE_WAITING);

    KeUnwaitThreadNoLock(pThread, STATUS_TIMEOUT, 0);

    KeReleaseSpinLockFromDpcLevel(&pThread->ThreadLock);
}

VOID
NTAPI
PspCreateThreadStacks(PKTHREAD tcb)
{
    /* Main kernel stack */
    tcb->NumberOfKernelStackPages = 4;
    tcb->OriginalKernelStackPointer =
        PspCreateKernelStack(tcb->NumberOfKernelStackPages);
    tcb->KernelStackPointer = (PVOID)(
        (ULONG_PTR)tcb->OriginalKernelStackPointer
        - sizeof(KTASK_STATE));

    /* Stack for saving the thread context when executing APCs */
    tcb->ApcBackupKernelStackPointer = PspCreateKernelStack(1);

    tcb->SwitchStackPointer = NULL;
}

NTSTATUS 
NTAPI
PspThreadOnCreateNoDispatcher(
    PVOID SelfObject,
    PVOID CreateData)
{
    ULONG_PTR userstack;
    PTHREAD_ON_CREATE_DATA threadCreationData;
    PETHREAD pThread = (PETHREAD)SelfObject;
    threadCreationData = (PTHREAD_ON_CREATE_DATA)CreateData;

    if (threadCreationData == NULL)
        return STATUS_INVALID_PARAMETER;

    KeAcquireSpinLockAtDpcLevel(
        &threadCreationData->ParentProcess->Pcb.ProcessLock);

    pThread->Tcb.ThreadState = THREAD_STATE_INITIALIZATION;
    pThread->Process = threadCreationData->ParentProcess;
    pThread->StartAddress = 0;
    KeInitializeSpinLock(&pThread->Tcb.ThreadLock);

    KeSetCustomThreadAddressSpace(
        KeGetCurrentThread(), 
        &pThread->Process->Pcb.AddressSpace);

    PspCreateThreadStacks(&pThread->Tcb);
    /* Assuming the memory manager works correctly (it doesn't yet), 
       this shouldn't fail. This condition is only false when there are no
       virtual addresses to be assigned (again, assuming a working Mm - for now
       it can fail when there are no PFNs to be allocated, and there is no way
       to find PFNs to page out. */
    ASSERT(pThread->Tcb.OriginalKernelStackPointer != NULL &&
           pThread->Tcb.ApcBackupKernelStackPointer != NULL);

    if (threadCreationData->IsKernel)
    {
        userstack = (ULONG_PTR)pThread->Tcb.KernelStackPointer;
    }
    else
    {
        /* TODO: Usermode should manage its stack on its own. */
        userstack = (ULONG_PTR)
        PagingAllocatePageFromRange(
            PAGING_USER_SPACE,
            PAGING_USER_SPACE_END) + PAGE_SIZE;
    }

    KeClearCustomThreadAddressSpace(KeGetCurrentThread());

    RtlZeroMemory(
        pThread->Tcb.ThreadWaitBlocks, 
        sizeof(pThread->Tcb.ThreadWaitBlocks));

    pThread->Tcb.ThreadPriority = 0;
    pThread->Tcb.NumberOfCurrentWaitBlocks = 0;
    pThread->Tcb.NumberOfActiveWaitBlocks = 0;
    pThread->Tcb.CurrentWaitBlocks = (PKWAIT_BLOCK)NULL;
    pThread->Tcb.WaitStatus = 0;
    pThread->Tcb.Alertable = FALSE;
    pThread->Tcb.Process = &pThread->Process->Pcb;
    pThread->Tcb.TimeoutEntry.Timeout = 0;
    pThread->Tcb.TimeoutEntry.TimeoutIsAbsolute = FALSE;
    pThread->Tcb.TimeoutEntry.OnTimeout = PspOnThreadTimeout;
    InitializeListHead(&pThread->Tcb.TimeoutEntry.ListEntry);
    pThread->Tcb.UserAffinity = 0;
    pThread->Tcb.ThreadIrql = PASSIVE_LEVEL;

    pThread->Tcb.CustomAddressSpace = NULL;

    /* Inherit affinity after the parent process */
    pThread->Tcb.Affinity = pThread->Tcb.Process->AffinityMask;

    PspSetupThreadState(
        (PKTASK_STATE)pThread->Tcb.KernelStackPointer, 
        threadCreationData->IsKernel,
        threadCreationData->Entrypoint, 
        userstack);

    InsertTailList(
        &pThread->Process->Pcb.ThreadListHead, 
        &pThread->Tcb.ProcessChildListEntry);

    KeReleaseSpinLockFromDpcLevel(
        &threadCreationData->ParentProcess->Pcb.ProcessLock);
    return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI
PspThreadOnDelete(
    PVOID selfObject)
{
    KIRQL irql;
    PETHREAD pThread = (PETHREAD)selfObject;

    irql = KiAcquireDispatcherLock();
    KeAcquireSpinLockAtDpcLevel(&pThread->Process->Pcb.ProcessLock);
    KeAcquireSpinLockAtDpcLevel(&pThread->Tcb.ThreadLock);

    pThread->Tcb.ThreadState = THREAD_STATE_TERMINATED;

    RemoveEntryList(&pThread->Tcb.ProcessChildListEntry);
    RemoveEntryList(&pThread->Tcb.ThreadListEntry);

    PspFreeKernelStack(
        pThread->Tcb.OriginalKernelStackPointer, 
        pThread->Tcb.NumberOfKernelStackPages);

    PspFreeKernelStack(
        pThread->Tcb.ApcBackupKernelStackPointer,
        1);

    KeReleaseSpinLockFromDpcLevel(&pThread->Tcb.ThreadLock);
    KeReleaseSpinLockFromDpcLevel(&pThread->Process->Pcb.ProcessLock);
    KiReleaseDispatcherLock(irql);

    return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI
PspProcessOnDelete(
    PVOID selfObject)
{
    KIRQL irql;
    PLIST_ENTRY current;
    PHANDLE_DATABASE currentHandleDatabase;
    PEPROCESS pProcess;

    pProcess = (PEPROCESS)selfObject;

    irql = KiAcquireDispatcherLock();
    KeAcquireSpinLockAtDpcLevel(&pProcess->Pcb.ProcessLock);

    current = pProcess->Pcb.ThreadListHead.First;
    while (current != &pProcess->Pcb.ThreadListHead)
    {
        /* The list of threads is not empty and somehow the process was 
         * dereferenced and is deleted. */
        KeBugCheckEx(
            CRITICAL_STRUCTURE_CORRUPTION,
            (ULONG_PTR)current, 
            __LINE__, 
            0, 
            0);
    }

    currentHandleDatabase = 
        (PHANDLE_DATABASE)pProcess->Pcb.HandleDatabaseHead.First;
    while (currentHandleDatabase != 
        (PHANDLE_DATABASE)&pProcess->Pcb.HandleDatabaseHead)
    {
        PHANDLE_DATABASE next;
        INT i;

        next = (PHANDLE_DATABASE)currentHandleDatabase->HandleDatabaseChainEntry.Next;

        for (i = 0; i < ENTRIES_PER_HANDLE_DATABASE; i++)
        {
            ObCloseHandleByEntry(&currentHandleDatabase->Entries[i]);
        }

        ExFreePool(currentHandleDatabase);
        currentHandleDatabase = next;
    }

    RemoveEntryList(&pProcess->Pcb.ProcessListEntry);

    // TODO: PagingDeleteAddressSpace(&pProcess->Pcb.AddressSpace);

    /* TODO: LdrOnProcessExit(pProcess); */

    KeReleaseSpinLockFromDpcLevel(&pProcess->Pcb.ProcessLock);
    KiReleaseDispatcherLock(irql);
    return STATUS_SUCCESS;
}

VOID
NTAPI
PspInsertIntoSharedQueueLocked(
    PKTHREAD Thread)
{
    KIRQL irql = KiAcquireDispatcherLock();
    PspInsertIntoSharedQueue(Thread);
    KiReleaseDispatcherLock(irql);
}

VOID 
NTAPI
PspInsertIntoSharedQueue(
    PKTHREAD Thread)
{
    UCHAR ThreadPriority;

    ASSERT(LOCKED(DispatcherLock));

    ThreadPriority = (UCHAR)(
        Thread->ThreadPriority 
            + (CHAR)Thread->Process->BasePriority);
    InsertTailList(
        &PspSharedReadyQueue.ThreadReadyQueues[ThreadPriority], 
        (PLIST_ENTRY)&Thread->ReadyQueueEntry);

    _bittestandset(
        &PspSharedReadyQueue.ThreadReadyQueuesSummary,
        ThreadPriority);
}

VOID
NTAPI
PsNotifyThreadAwaken()
{
    KeSendIpi(KAFFINITY_ALL, SCHEDULER_NOTIFY_IPI_VECTOR);
}

BOOLEAN
NTAPI
PspManageSharedReadyQueue(
    UCHAR CoreNumber)
{
    PKCORE_SCHEDULER_DATA coreSchedulerData;
    INT checkedPriority;
    BOOLEAN result;
    PKTHREAD thread;
    PLIST_ENTRY sharedReadyQueues, coreReadyQueues;

    result = FALSE;
    coreSchedulerData = &CoresSchedulerData[CoreNumber];

    sharedReadyQueues = PspSharedReadyQueue.ThreadReadyQueues;
    coreReadyQueues = coreSchedulerData->ThreadReadyQueues;

    /* If there are no threads in the shared queue, 
     * don't bother with locking it and just return. */
    if (PspSharedReadyQueue.ThreadReadyQueuesSummary == 0)
    {
        return FALSE;
    }

    /* If the local ready queue has a thread with a higher priority than the
     * shared queue, do not try to get threads from the shared queue. */
    if (coreSchedulerData->ThreadReadyQueuesSummary >
        (PspSharedReadyQueue.ThreadReadyQueuesSummary ^
        coreSchedulerData->ThreadReadyQueuesSummary))
    {
        return result;
    }

    for (checkedPriority = 31; 
        checkedPriority >= 0 && !result;
        checkedPriority--)
    {
        PLIST_ENTRY current;
         
        /* If this priority doesn't have any threads in the shared queue. */
        if (!_bittest(
                &PspSharedReadyQueue.ThreadReadyQueuesSummary,
                checkedPriority))
        {
            continue;
        }
        current = sharedReadyQueues[checkedPriority].First;
                
        while (current != &sharedReadyQueues[checkedPriority])
        {
            thread = CONTAINING_RECORD(current, KTHREAD, ReadyQueueEntry);

            /* Check if this processor can even run this thread. */
            if (thread->Affinity & (1LL << CoreNumber))
            {
                RemoveEntryList((PLIST_ENTRY)current);
                InsertTailList(
                    &coreReadyQueues[checkedPriority], 
                    (PLIST_ENTRY)current);
                result = TRUE;

                /* If this was the first thread with this priority in the thread
                 * queue, adjust the summary accordingly. */
                SetSummaryBitIfNeccessary(
                    coreReadyQueues, 
                    &coreSchedulerData->ThreadReadyQueuesSummary,
                    checkedPriority);

                /* If this was the last thread with this priority in the shared
                 * queue, adjust its summary accordingly. */
                ClearSummaryBitIfNeccessary(
                    sharedReadyQueues, 
                    &PspSharedReadyQueue.ThreadReadyQueuesSummary, 
                    checkedPriority);

                break;
            }

            current = current->Next;
        }
    }

    return result;
}

__declspec(noreturn) 
VOID 
NTAPI
PsExitThread(
    DWORD exitCode)
{
    PKTHREAD currentThread;
    KIRQL originalIrql;

    originalIrql = KfRaiseIrql(DISPATCH_LEVEL);

    currentThread = KeGetCurrentThread();
    KeAcquireSpinLockAtDpcLevel(&currentThread->Header.Lock);
    KiAcquireDispatcherLock();
    KeAcquireSpinLockAtDpcLevel(&currentThread->ThreadLock);
    currentThread->ThreadState = THREAD_STATE_TERMINATED;
    currentThread->ThreadExitCode = exitCode;

    KiSignal(&currentThread->Header, -1, 0);

    KeReleaseSpinLockFromDpcLevel(&currentThread->Header.Lock);
    KeReleaseSpinLockFromDpcLevel(&currentThread->ThreadLock);
    KeReleaseSpinLockFromDpcLevel(&DispatcherLock);

    /* TODO: Handle sections. */

    HalDisableInterrupts();
    KeLowerIrql(originalIrql);
    KeForceClockTick();
    ASSERT(FALSE);
}

BOOL
NTAPI
KiSetUserMemory(
    PVOID Address,
    ULONG_PTR Data)
{
    /* TODO: do checks before setting the data */
    *((ULONG_PTR*)Address) = Data;
    return TRUE;
}

/* TODO: implement a PspGetUsercallParameter maybe? */
NTSTATUS
NTAPI
PspSetUsercallParameter(
    PKTHREAD pThread,
    ULONG ParameterIndex,
    ULONG_PTR Value)
{
#ifdef _M_AMD64
    PKTASK_STATE pTaskState;

    pTaskState = (PKTASK_STATE)pThread->KernelStackPointer;

    switch (ParameterIndex)
    {
    case 0:
        pTaskState->Rcx = Value;
        break;
    case 1:
        pTaskState->Rdx = Value;
        break;
    case 2:
        pTaskState->R8 = Value;
        break;
    case 3:
        pTaskState->R9 = Value;
        break;
    default:
    {
        /* This is called after allocating the shadow space 
         * and the return address. */
        ULONG_PTR stackLocation = pTaskState->Rsp;

        /* Skip the return address and shadow space. */
        stackLocation += 4 * sizeof(ULONG_PTR) + 1 * sizeof(PVOID);

        /* Parameter's relative stack location. */
        stackLocation += sizeof(ULONG_PTR) * (ParameterIndex - 4);

        KiSetUserMemory((PVOID)stackLocation, Value);
        return STATUS_INVALID_ADDRESS;
    }
    }
    return STATUS_SUCCESS;
#else
#error Unimplemented
#endif
}

VOID
NTAPI
PspUsercall(
    PKTHREAD pThread,
    PVOID Function,
    ULONG_PTR* Parameters,
    SIZE_T NumberOfParameters,
    PVOID ReturnAddress)
{
#ifdef _M_AMD64
    PKTASK_STATE pTaskState;
    INT i;

    pTaskState = (PKTASK_STATE)pThread->KernelStackPointer;

    /* Allocate stack space for the registers that don't fit onto the stack. */
    if (NumberOfParameters > 4)
    {
        pTaskState->Rsp -= (NumberOfParameters - 4) * sizeof(ULONG_PTR);
    }

    /* Allocate the shadow space. */
    pTaskState->Rsp -= 4 * sizeof(ULONG_PTR);

    /* Allocate the return address. */
    pTaskState->Rsp -= sizeof(PVOID);
    KiSetUserMemory((PVOID)pTaskState->Rsp, (ULONG_PTR)ReturnAddress);

    for (i = 0; i < NumberOfParameters; i++)
    {
        PspSetUsercallParameter(pThread, i, Parameters[i]);
    }

#else
#error Unimplemented
#endif 
}

KPROCESSOR_MODE
NTAPI
PsGetProcessorModeFromTrapFrame(
    PKTASK_STATE TrapFrame)
{
    if ((TrapFrame->Cs & 0x3) == 0x00)
    {
        return KernelMode;
    }
    return UserMode;
}

VOID
NTAPI
KeSetSystemAffinityThread(
    KAFFINITY Affinity)
{
    PKTHREAD pThread;
    KIRQL irql = KiAcquireDispatcherLock();

    pThread = KeGetCurrentThread();
    pThread->UserAffinity = pThread->Affinity;
    pThread->Affinity = Affinity;

    KiReleaseDispatcherLock(irql);
    KeForceClockTick();
}

VOID
NTAPI
KeRevertToUserAffinityThread()
{
    PKTHREAD pThread;
    KIRQL irql = KiAcquireDispatcherLock();

    pThread = KeGetCurrentThread();
    pThread->Affinity = pThread->UserAffinity;
    pThread->UserAffinity = 0;

    KiReleaseDispatcherLock(irql);
    KeForceClockTick();
}

PEPROCESS
NTAPI
KeGetCurrentProcess()
{
    if (KeGetCurrentThread() == NULL)
    {
        return NULL;
    }
    return CONTAINING_RECORD(KeGetCurrentThread()->Process, EPROCESS, Pcb);
}

NTSTATUS
NTAPI
KeClearCustomThreadAddressSpace(
    PKTHREAD pThread)
{
    ASSERT(pThread != NULL);

    //PrintT("[%i] Clearing %X-%X\n", KeGetCurrentProcessorId(), pThread, pThread->CustomAddressSpace);
    pThread->CustomAddressSpace = NULL;
    //PrintT("[%i] Cleared %X-%X\n", KeGetCurrentProcessorId(), pThread, pThread->CustomAddressSpace);
    if (pThread == KeGetCurrentThread())
    {
        MmApplyAddressSpace(&pThread->Process->AddressSpace);
    }
    else
    {
        /* TODO: Notify core running the thread, if any. */
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
KeSetCustomThreadAddressSpace(
    PKTHREAD pThread,
    PADDRESS_SPACE pAddressSpace)
{
    ASSERT(pThread != NULL && pAddressSpace != NULL);

    //PrintT("[%i] Setting %X-%X\n", KeGetCurrentProcessorId(), pThread, pThread->CustomAddressSpace);
    pThread->CustomAddressSpace = pAddressSpace;
    //PrintT("[%i] Set %X-%X\n", KeGetCurrentProcessorId(), pThread, pThread->CustomAddressSpace);
    if (pThread == KeGetCurrentThread())
    {
        MmApplyAddressSpace(pAddressSpace);
    }

    return STATUS_SUCCESS;
}

NTSYSAPI
NTSTATUS
NTAPI
NtCreateProcessEx(
    PHANDLE pHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES pObjectAttributes,
    HANDLE pParentProcess,
    ULONG Flags,
    HANDLE SectionHandle,
    HANDLE DebugPort,
    HANDLE ExceptionPort,
    BOOL InJob)
{
    PEPROCESS pProcess;
    NTSTATUS Status;
    HANDLE Handle;

    /* TODO: passing pObjectAttributes here is bullshit, we shouldn't pass
     * the name - the process is not a named object. Could this work, because
     * it has no parent?? 
     * 
     * TODO2: What's the root in the attributes anyway? */
    Status = PspCreateProcessInternal(
        &pProcess, 
        DesiredAccess,
        pObjectAttributes);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = ObCreateHandle(
        &Handle, 
        DesiredAccess, /* TODO: Should this be passed for the second time?? 
                          Can handles have different access than the objects 
                          they refer to? */
        FALSE,
        pProcess);
    if (!NT_SUCCESS(Status))
    {
        ObDereferenceObject(pProcess);
        return Status;
    }

    /* TODO: copy object table, do all the other fancy stuff with arguments */

    *pHandle = Handle;
    return STATUS_SUCCESS;
}

NTSYSAPI
NTSTATUS
NTAPI
NtCreateProcess(
    PHANDLE Handle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    HANDLE ParentProcess,
    BOOLEAN InheritObjectTable,
    HANDLE SectionHandle,
    HANDLE DebugPort,
    HANDLE ExceptionPort)
{
    ULONG Flags = 0;

    if (InheritObjectTable)
    {
        Flags |= 0x00000004L;
    }

    return NtCreateProcessEx(
        Handle,
        DesiredAccess,
        ObjectAttributes,
        ParentProcess,
        Flags,
        SectionHandle,
        DebugPort,
        ExceptionPort,
        FALSE);
}

NTSYSAPI
NTSTATUS
NTAPI
NtCreateThread(
    PHANDLE ThreadHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    HANDLE ProcessHandle,
    PVOID ClientId, /* TODO */
    PCONTEXT Context /* FIXME: Context ignored, incorrect definition! */,
    PVOID InitialTeb, /* TODO */
    BOOLEAN CreateSuspended)
{
    NTSTATUS Status;
    PEPROCESS pProcess;
    PETHREAD pThread;
    HANDLE handle;

    if (ProcessHandle == NULL)
    {
        pProcess = CONTAINING_RECORD(KeGetCurrentThread()->Process, 
                                     EPROCESS, 
                                     Pcb);
    }
    else
    {
        Status = ObReferenceObjectByHandle(ProcessHandle,
                                           0,
                                           PsProcessType,
                                           UserMode,
                                           &pProcess,
                                           NULL);
        if (!NT_SUCCESS(Status))
        {
            return Status;
        }
    }

    /* FIXME! Context ignored! */
    Status = PspCreateThreadInternal(&pThread, pProcess, FALSE, Context->Rip);
    if (ProcessHandle != NULL)
    {
        ObDereferenceObject(pProcess);
    }
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    if (!CreateSuspended)
    {
        PspInsertIntoSharedQueueLocked(&pThread->Tcb);
    }

    Status = ObCreateHandle(&handle, DesiredAccess, FALSE, pThread);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    return STATUS_SUCCESS;
}