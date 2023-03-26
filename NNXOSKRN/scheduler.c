#include <scheduler.h>
#include <pool.h>
#include <paging.h>
#include <SimpleTextIo.h>
#include <MemoryOperations.h>
#include <physical_allocator.h>
#include <pcr.h>
#include <bugcheck.h>
#include <cpu.h>
#include <object.h>
#include <dispatcher.h>
#include <apc.h>
#include <ntdebug.h>

LIST_ENTRY ProcessListHead;
LIST_ENTRY ThreadListHead;

UCHAR PspForegroundQuantum[3] = { 0x06, 0x0C, 0x12 };
UCHAR PspPrioritySeparation = 2;
UINT PspCoresInitialized = 0;

const CHAR PspThrePoolTag[4] = "Thre";
const CHAR PspProcPoolTag[4] = "Proc";
POBJECT_TYPE PsProcessType = NULL;
POBJECT_TYPE PsThreadType = NULL;

extern ULONG_PTR KeMaximumIncrement;
extern ULONG_PTR KiCyclesPerClockQuantum;
extern ULONG_PTR KiCyclesPerQuantum;
extern UINT KeNumberOfProcessors;

VOID PspSetupThreadState(PKTASK_STATE pThreadState, BOOL IsKernel, ULONG_PTR EntryPoint, ULONG_PTR Userstack);
NTSTATUS PspCreateProcessInternal(PEPROCESS* ppProcess);
NTSTATUS PspCreateThreadInternal(PETHREAD* ppThread, PEPROCESS pParentProcess, BOOL IsKernel, ULONG_PTR EntryPoint);
__declspec(noreturn) VOID PspTestAsmUser();
__declspec(noreturn) VOID PspTestAsmUserEnd();
__declspec(noreturn) VOID PspTestAsmUser2();
__declspec(noreturn) VOID PspIdleThreadProcedure();
BOOL TestBit(ULONG_PTR Number, ULONG_PTR BitIndex);
ULONG_PTR ClearBit(ULONG_PTR Number, ULONG_PTR BitIndex);
ULONG_PTR SetBit(ULONG_PTR Number, ULONG_PTR BitIndex);
NTSTATUS PspProcessOnCreate(PVOID SelfObject, PVOID CreateData);
NTSTATUS PspProcessOnCreateNoDispatcher(PVOID SelfObject, PVOID CreateData);
NTSTATUS PspProcessOnDelete(PVOID SelfObject);
NTSTATUS PspThreadOnCreate(PVOID SelfObject, PVOID CreateData);
NTSTATUS PspThreadOnCreateNoDispatcher(PVOID SelfObject, PVOID CreateData);
NTSTATUS PspThreadOnDelete(PVOID SelfObject);

struct _READY_QUEUES
{
    /* a ready queue for each thread priority */
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

/* clear the summary bit for this priority if there are none entries left */
inline VOID ClearSummaryBitIfNeccessary(
    LIST_ENTRY* ThreadReadyQueues, 
    PULONG Summary,
    UCHAR Priority)
{
    if (IsListEmpty(&ThreadReadyQueues[Priority]))
    {
        *Summary = (ULONG)ClearBit(
            *Summary,
            Priority
        );
    }
}

inline VOID SetSummaryBitIfNeccessary(
    LIST_ENTRY* ThreadReadyQueues, 
    PULONG Summary, 
    UCHAR Priority)
{
    if (!IsListEmpty(&ThreadReadyQueues[Priority]))
    {
        *Summary = (ULONG)SetBit(
            *Summary,
            Priority);
    }
}

NTSTATUS PspInitializeScheduler()
{
    INT i;
    KIRQL irql;
    NTSTATUS status;

    status = ObCreateSchedulerTypes(
        &PsProcessType, 
        &PsThreadType
    );

    PsProcessType->OnCreate = PspProcessOnCreate;
    PsProcessType->OnDelete = PspProcessOnDelete;
    PsThreadType->OnCreate = PspThreadOnCreate;
    PsThreadType->OnDelete = PspThreadOnDelete;

    irql = KiAcquireDispatcherLock();

    if (CoresSchedulerData == NULL)
    {
        InitializeListHead(&ThreadListHead);
        InitializeListHead(&ProcessListHead);

        CoresSchedulerData = (PKCORE_SCHEDULER_DATA)PagingAllocatePageBlockFromRange(
            (KeNumberOfProcessors * sizeof(KCORE_SCHEDULER_DATA) + PAGE_SIZE - 1) / PAGE_SIZE,
            PAGING_KERNEL_SPACE,
            PAGING_KERNEL_SPACE_END
        );
            
        if (CoresSchedulerData != NULL)
        {
            for (i = 0; i < 32; i++)
            {
                InitializeListHead(&PspSharedReadyQueue.ThreadReadyQueues[i]);
            }
        }
    }
    KiReleaseDispatcherLock(irql);
    return (CoresSchedulerData == (PKCORE_SCHEDULER_DATA)NULL) ? (STATUS_NO_MEMORY) : (STATUS_SUCCESS);
}

PKTHREAD PspSelectNextReadyThread(UCHAR CoreNumber)
{
    INT priority;
    PKTHREAD result;
    PKCORE_SCHEDULER_DATA coreOwnData = &CoresSchedulerData[CoreNumber];

    PspManageSharedReadyQueue(CoreNumber);
    result = &coreOwnData->IdleThread->Tcb;

    /* start with the highest priority */
    for (priority = 31; priority >= 0; priority--)
    {
        if (TestBit(coreOwnData->ThreadReadyQueuesSummary, priority))
        {
            PLIST_ENTRY dequeuedEntry = (PLIST_ENTRY)RemoveHeadList(&coreOwnData->ThreadReadyQueues[priority]);
            result = (PKTHREAD)((ULONG_PTR)dequeuedEntry - FIELD_OFFSET(KTHREAD, ReadyQueueEntry));

            ClearSummaryBitIfNeccessary(coreOwnData->ThreadReadyQueues, &coreOwnData->ThreadReadyQueuesSummary, priority);
            break;
        }
    }

    /* if no process was found on queue(s), return the idle thread */
    return result;
}

NTSTATUS PspCreateIdleProcessForCore(
    PEPROCESS* outIdleProcess, 
    PETHREAD* outIdleThread, 
    UINT8 coreNumber) 
{
    NTSTATUS status;
    PEPROCESS pIdleProcess;
    PETHREAD pIdleThread;
    THREAD_ON_CREATE_DATA threadCreationData;

    /**
     * Since DispatcherLock is already held, we cannot call the ObjectManager to create the idle thread.
     * This isn't a problem, because the idle thread cannot be dereferenced or deleted, and as such
     * can be manually created, without deadlock due to trying to acquire a lock already held bu this
     * core. 
     */

    /* allocate memory for the process structure */
    pIdleProcess = ExAllocatePool(NonPagedPool, sizeof(*pIdleProcess));
    if (pIdleProcess == NULL)
        return STATUS_NO_MEMORY;

    /* initialize the process structure */
    status = PspProcessOnCreateNoDispatcher((PVOID)pIdleProcess, NULL);
    if (!NT_SUCCESS(status))
    {
        ExFreePool(pIdleProcess);
        return status;
    }
    pIdleProcess->Pcb.AffinityMask = (1ULL << (ULONG_PTR)coreNumber);

    /* allocate memory fot the thread structure */
    pIdleThread = ExAllocatePool(NonPagedPool, sizeof(*pIdleThread));
    if (pIdleThread == NULL)
    {
        ExFreePool(pIdleProcess);
        return STATUS_NO_MEMORY;
    }

    /** 
     * initialize the thread creation data structure, 
     * which is neccessary to initialize the thread 
     */
    threadCreationData.Entrypoint = (ULONG_PTR)PspIdleThreadProcedure;
    threadCreationData.IsKernel = TRUE;
    threadCreationData.ParentProcess = pIdleProcess;

    /* initialize the thread structure */
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

    /* add the idle process and the idle thread to their respective lists */
    InsertHeadList(&ProcessListHead, &pIdleProcess->Pcb.ProcessListEntry);
    InsertHeadList(&ThreadListHead, &pIdleThread->Tcb.ThreadListEntry);

    *outIdleProcess = pIdleProcess;
    *outIdleThread = pIdleThread;

    return STATUS_SUCCESS;
}

VOID PspInitializeCoreSchedulerData(UINT8 CoreNumber)
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

    /* check if this is the first core to be initialized */
    /* if so, initialize the scheduler first */
    if ((PVOID)CoresSchedulerData == NULL)
    {
        status = PspInitializeScheduler();
        
        if (status)
            return status;
    }

    KeRaiseIrql(DISPATCH_LEVEL, &irql);
    PspInitializeCoreSchedulerData(CoreNumber);
    KeLowerIrql(irql);

    irql = KiAcquireDispatcherLock();

    pPcr = KeGetPcr();
    pPrcb = pPcr->Prcb;
    KeAcquireSpinLockAtDpcLevel(&pPrcb->Lock);
#pragma warning(disable: 6011)
    pPrcb->IdleThread = &CoresSchedulerData[CoreNumber].IdleThread->Tcb;
    pPrcb->NextThread = pPrcb->IdleThread;
    pPrcb->CurrentThread = pPrcb->IdleThread;
    pPcr->CyclesLeft = (LONG_PTR)KiCyclesPerQuantum * 100;
    KeReleaseSpinLockFromDpcLevel(&pPrcb->Lock);

    PspCoresInitialized++;
    
    PagingSetAddressSpace(
        pPrcb->IdleThread->Process->AddressSpacePhysicalPointer);

    pPrcb->IdleThread->ThreadState = THREAD_STATE_RUNNING;
    KiReleaseDispatcherLock(irql);

    if (PspCoresInitialized == KeNumberOfProcessors)
    {
        NTSTATUS status;
#if 0
        NTSTATUS ObpMpTest();

        PrintT("Running mp test for core %i\n", KeGetCurrentProcessorId());
        status = ObpMpTest();
        if (!NT_SUCCESS(status))
        {
            return status;
        }
#endif

        for (int i = 0; i < 1; i++)
        {
            PEPROCESS userProcess1;
            PETHREAD userThread1, userThread2;

            VOID TestUserThread1();
            VOID TestUserThread2();

            status = PspCreateProcessInternal(&userProcess1);
            userProcess1->Pcb.AffinityMask = KAFFINITY_ALL;
            if (!NT_SUCCESS(status))
            {
                return status;
            }

            status = PspCreateThreadInternal(
                &userThread1,
                userProcess1,
                FALSE,
                (ULONG_PTR)TestUserThread1
            );
            if (!NT_SUCCESS(status))
            {
                return status;
            }

            status = PspCreateThreadInternal(
                &userThread2,
                userProcess1,
                FALSE,
                (ULONG_PTR)TestUserThread2
            );
            if (!NT_SUCCESS(status))
            {
                return status;
            }

            userThread1->Tcb.ThreadPriority = 1;
            userThread2->Tcb.ThreadPriority = 1;
            PspInsertIntoSharedQueueLocked((PKTHREAD)userThread1);
            PspInsertIntoSharedQueueLocked((PKTHREAD)userThread2);
            PrintT("Userthreads: %X %X\n", userThread1, userThread2);
        }
    }

    PKTASK_STATE pTaskState = pPrcb->IdleThread->KernelStackPointer;
    
    PagingSetAddressSpace(
        pPrcb->IdleThread->Process->AddressSpacePhysicalPointer);
    
    HalDisableInterrupts();
    KeLowerIrql(0);
    
    PrintT(
        "Core %i scheduler initialized %i\n", 
        KeGetCurrentProcessorId(), 
        KeGetCurrentIrql());

    HalpApplyTaskState(pTaskState);

    return STATUS_SUCCESS;
}

#pragma warning(pop)

ULONG_PTR 
NTAPI
PspScheduleThread(PKINTERRUPT ClockInterrupt, PKTASK_STATE Stack)
{
    PKPCR pcr;
    KIRQL irql;

    PKSPIN_LOCK originalRunningProcessSpinlock;
    PKTHREAD nextThread;
    PKTHREAD originalRunningThread;
    PKPROCESS originalRunningProcess;
    UCHAR origRunThrdPriority;

    irql = KiAcquireDispatcherLock();
    KiClockTick();
    pcr = KeGetPcr();

    KeAcquireSpinLockAtDpcLevel(&pcr->Prcb->Lock);

    originalRunningThread = pcr->Prcb->CurrentThread;
    
    if (originalRunningThread == NULL)
    {
        KeBugCheckEx(
            WORKER_THREAD_TEST_CONDITION, 
            (ULONG_PTR)originalRunningThread, 
            0, 0, 0);
    }

    originalRunningThread->KernelStackPointer = (PVOID)Stack;

    if (originalRunningThread->ThreadState != THREAD_STATE_RUNNING)
    {
        pcr->CyclesLeft = 0;
    }

    /* The thread has IRQL too high for the scheduler, the clock interrupt
     * should only update the clock state. */
    if (originalRunningThread->ThreadIrql >= DISPATCH_LEVEL)
    {
        KeReleaseSpinLockFromDpcLevel(&pcr->Prcb->Lock);
        KiReleaseDispatcherLock(irql);
        return (ULONG_PTR)originalRunningThread->KernelStackPointer;
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
                if (pcr->Prcb->CurrentThread->Process != 
                    pcr->Prcb->NextThread->Process)
                {
                    PagingSetAddressSpace(
                        (ULONG_PTR)nextThread->Process->AddressSpacePhysicalPointer);
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
            else if (originalRunningThread->ThreadState == THREAD_STATE_TERMINATED)
            {
                /* TODO */
                /* FIXME: Terminated threads cause a significant memory leak,
                 * they cannot, however, be dereferenced here, as the IRQL is
                 * too high to use the Object Manager. */
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
    KiReleaseDispatcherLock(irql);

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

PVOID PspCreateKernelStack(SIZE_T nPages)
{
    ULONG_PTR blockAllocation, currentMappingGuard1, currentMappingGuard2;
    NTSTATUS status;

    /* Allocate two more pages for the page guard. */
    blockAllocation = PagingAllocatePageBlockFromRange(
        nPages + 2, 
        PAGING_KERNEL_SPACE, 
        PAGING_KERNEL_SPACE_END
    );

    /* Get the current mappings. */
    currentMappingGuard1 = 
        PagingGetCurrentMapping(blockAllocation)
        & PAGE_ADDRESS_MASK;
    currentMappingGuard2 = 
        PagingGetCurrentMapping(blockAllocation + PAGE_SIZE * (nPages + 1))
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
PspFreeKernelStack(
    PVOID OriginalStackLocation,
    SIZE_T nPages
)
{
    /* TODO */
}

ULONG_PTR GetRSP();

VOID 
NTAPI
PspSetupThreadState(
    PKTASK_STATE pThreadState, 
    BOOL IsKernel, 
    ULONG_PTR Entrypoint,
    ULONG_PTR Userstack
)
{
    UINT16 code0, code3, data0, data3;
    LPKGDTENTRY64 gdt;

    gdt = KeGetPcr()->Gdt;

    code0 = HalpGdtFindEntry(gdt, 7, TRUE, FALSE);
    code3 = HalpGdtFindEntry(gdt, 7, TRUE, TRUE);

    data0 = HalpGdtFindEntry(gdt, 7, FALSE, FALSE);
    data3 = HalpGdtFindEntry(gdt, 7, FALSE, TRUE);

    MemSet(pThreadState, 0, sizeof(*pThreadState));
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

NTSTATUS PspCreateProcessInternal(PEPROCESS* ppProcess)
{
    NTSTATUS status;
    PEPROCESS pProcess;
    OBJECT_ATTRIBUTES processObjAttributes;

    /* Create the process object. */
    InitializeObjectAttributes(
        &processObjAttributes,
        NULL,
        OBJ_KERNEL_HANDLE,
        INVALID_HANDLE_VALUE,
        NULL
    );

    status = ObCreateObject(
        &pProcess, 
        0, 
        KernelMode, 
        &processObjAttributes, 
        sizeof(EPROCESS), 
        PsProcessType,
        NULL
    );

    if (status != STATUS_SUCCESS)
        return status;

    *ppProcess = pProcess;

    return STATUS_SUCCESS;
}


NTSTATUS PspProcessOnCreate(PVOID selfObject, PVOID createData)
{
    PEPROCESS pProcess;
    NTSTATUS status;
    KIRQL irql;

    pProcess = (PEPROCESS)selfObject;

    /* acquire the dispatcher lock */
    irql = KiAcquireDispatcherLock();

    /* initialize the dispatcher header */
    InitializeDispatcherHeader(&pProcess->Pcb.Header, ProcessObject);

    /* initialize the process structure */
    status = PspProcessOnCreateNoDispatcher(selfObject, createData);
    /* add the process to the process list */
    InsertTailList(&ProcessListHead, &pProcess->Pcb.ProcessListEntry);

    /* release the dispatcher lock */
    KiReleaseDispatcherLock(irql);

    return status;
}

NTSTATUS PspProcessOnCreateNoDispatcher(PVOID selfObject, PVOID createData)
{
    PEPROCESS pProcess = (PEPROCESS)selfObject;

    /* make sure it is not prematurely used */
    pProcess->Initialized = FALSE;
    KeInitializeSpinLock(&pProcess->Pcb.ProcessLock);
    KeInitializeSpinLock(&pProcess->Pcb.ProcessLock);
    InitializeListHead(&pProcess->Pcb.ThreadListHead);
    pProcess->Pcb.BasePriority = 0;
    pProcess->Pcb.AffinityMask = KAFFINITY_ALL;
    pProcess->Pcb.NumberOfThreads = 0;
    pProcess->Pcb.AddressSpacePhysicalPointer = PagingCreateAddressSpace();
    pProcess->Pcb.QuantumReset = 6;
    InitializeListHead(&pProcess->Pcb.HandleDatabaseHead);

    return STATUS_SUCCESS;
}

/**
 * @brief Allocates memory for a new thread, adds it to the scheduler's thread list and parent process' child thread list
 * @param ppThread pointer to a pointer to PETHREAD, value it's pointing to will be set to result of allocation after this function
 * @param pParentProcess pointer to EPROCESS structure of the parent process for this thread
 * @param IsKernel if true, thread is created in kernel mode
 * @param EntryPoint entrypoint function for the thread, caller is responsible for making any neccessary changes in the parent process' address space 
 * @return STATUS_SUCCESS, STATUS_NO_MEMORY
*/
NTSTATUS PspCreateThreadInternal(
    PETHREAD* ppThread, 
    PEPROCESS pParentProcess, 
    BOOL IsKernel, 
    ULONG_PTR EntryPoint
)
{
    NTSTATUS status;
    PETHREAD pThread;
    OBJECT_ATTRIBUTES threadObjAttributes;
    THREAD_ON_CREATE_DATA data;

    InitializeObjectAttributes(
        &threadObjAttributes,
        NULL,
        OBJ_KERNEL_HANDLE,
        INVALID_HANDLE_VALUE,
        NULL
    );

    data.Entrypoint = EntryPoint;
    data.ParentProcess = pParentProcess;
    data.IsKernel = IsKernel;

    status = ObCreateObject(
        &pThread,
        0,
        KernelMode,
        &threadObjAttributes,
        sizeof(ETHREAD),
        PsThreadType,
        &data
    );

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
NTSTATUS PspThreadOnCreate(PVOID SelfObject, PVOID CreateData)
{
    KIRQL irql;
    NTSTATUS status;
    PETHREAD pThread;
    
    pThread = (PETHREAD)SelfObject;

    /* acquire the dispatcher lock */
    irql = KiAcquireDispatcherLock();

    /* initialize the dispatcher header */
    InitializeDispatcherHeader(&pThread->Tcb.Header, ThreadObject);

    /* initialize the thread structure */
    status = PspThreadOnCreateNoDispatcher(SelfObject, CreateData);

    /**
     * initialize the parts of the thread structure 
     * that are "protected" by the dispatcher lock 
     */
    InsertTailList(&ThreadListHead, &pThread->Tcb.ThreadListEntry);
    KeInitializeApcState(&pThread->Tcb.ApcState);
    KeInitializeApcState(&pThread->Tcb.SavedApcState);
    pThread->Tcb.ThreadState = THREAD_STATE_READY;

    /* release the lock and return */
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

NTSTATUS 
NTAPI
PspThreadOnCreateNoDispatcher(PVOID SelfObject, PVOID CreateData)
{
    ULONG_PTR userstack;
    ULONG_PTR originalAddressSpace;
    PTHREAD_ON_CREATE_DATA threadCreationData;
    PETHREAD pThread = (PETHREAD)SelfObject;
    threadCreationData = (PTHREAD_ON_CREATE_DATA)CreateData;

    if (threadCreationData == NULL)
        return STATUS_INVALID_PARAMETER;

    KeAcquireSpinLockAtDpcLevel(&threadCreationData->ParentProcess->Pcb.ProcessLock);

    pThread->Tcb.ThreadState = THREAD_STATE_INITIALIZATION;
    pThread->Process = threadCreationData->ParentProcess;
    pThread->StartAddress = 0;
    KeInitializeSpinLock(&pThread->Tcb.ThreadLock);

    originalAddressSpace = PagingGetAddressSpace();
    PagingSetAddressSpace(pThread->Process->Pcb.AddressSpacePhysicalPointer);

    /* Create stacks */
    /* Main kernel stack */
    pThread->Tcb.NumberOfKernelStackPages = 4;
    pThread->Tcb.OriginalKernelStackPointer = 
        PspCreateKernelStack(pThread->Tcb.NumberOfKernelStackPages);
    pThread->Tcb.KernelStackPointer = 
        (PVOID)((ULONG_PTR)pThread->Tcb.OriginalKernelStackPointer - sizeof(KTASK_STATE));

    /* Stack for saving the thread context when executing APCs */
    pThread->Tcb.ApcBackupKernelStackPointer = PspCreateKernelStack(1);

    /* allocate stack even if in kernel mode */
    if (threadCreationData->IsKernel)
        userstack = (ULONG_PTR)pThread->Tcb.KernelStackPointer;
    else
        userstack = (ULONG_PTR)
            PagingAllocatePageFromRange(PAGING_USER_SPACE, PAGING_USER_SPACE_END) + PAGE_SIZE;

    PagingSetAddressSpace(originalAddressSpace);

    MemSet(pThread->Tcb.ThreadWaitBlocks, 0, sizeof(pThread->Tcb.ThreadWaitBlocks));
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

    /* Inherit affinity after the parent process */
    pThread->Tcb.Affinity = pThread->Tcb.Process->AffinityMask;
    PspSetupThreadState((PKTASK_STATE)pThread->Tcb.KernelStackPointer, threadCreationData->IsKernel, threadCreationData->Entrypoint, userstack);
    InsertTailList(&pThread->Process->Pcb.ThreadListHead, &pThread->Tcb.ProcessChildListEntry);
    KeReleaseSpinLockFromDpcLevel(&threadCreationData->ParentProcess->Pcb.ProcessLock);
    return STATUS_SUCCESS;
}

NTSTATUS PspThreadOnDelete(PVOID selfObject)
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
        pThread->Tcb.NumberOfKernelStackPages
    );

    PspFreeKernelStack(
        pThread->Tcb.ApcBackupKernelStackPointer,
        1
    );

    KeReleaseSpinLockFromDpcLevel(&pThread->Tcb.ThreadLock);
    KeReleaseSpinLockFromDpcLevel(&pThread->Process->Pcb.ProcessLock);
    KiReleaseDispatcherLock(irql);

    return STATUS_SUCCESS;
}

NTSTATUS PspProcessOnDelete(PVOID selfObject)
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
        /* the list of threads is not empty and somehow the process was dereferenced */
        KeBugCheckEx(CRITICAL_STRUCTURE_CORRUPTION, (ULONG_PTR)current, __LINE__, 0, 0);
    }

    currentHandleDatabase = (PHANDLE_DATABASE)pProcess->Pcb.HandleDatabaseHead.First;
    while (currentHandleDatabase != (PHANDLE_DATABASE)&pProcess->Pcb.HandleDatabaseHead)
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

    MmFreePfn(PFN_FROM_PA(pProcess->Pcb.AddressSpacePhysicalPointer));

    KeReleaseSpinLockFromDpcLevel(&pProcess->Pcb.ProcessLock);
    KiReleaseDispatcherLock(irql);
    return STATUS_SUCCESS;
}

VOID
NTAPI
PspInsertIntoSharedQueueLocked(PKTHREAD Thread)
{
    KIRQL irql = KiAcquireDispatcherLock();
    PspInsertIntoSharedQueue(Thread);
    KiReleaseDispatcherLock(irql);
}

VOID 
NTAPI
PspInsertIntoSharedQueue(PKTHREAD Thread)
{
    UCHAR ThreadPriority;

    if (DispatcherLock == 0)
    {
        KeBugCheckEx(SPIN_LOCK_NOT_OWNED, __LINE__, 0, 0, 0);
    }

    ThreadPriority = (UCHAR)(Thread->ThreadPriority + (CHAR)Thread->Process->BasePriority);
    InsertTailList(&PspSharedReadyQueue.ThreadReadyQueues[ThreadPriority], (PLIST_ENTRY)&Thread->ReadyQueueEntry);
    PspSharedReadyQueue.ThreadReadyQueuesSummary = (ULONG)SetBit(
        PspSharedReadyQueue.ThreadReadyQueuesSummary,
        ThreadPriority
    );
}

BOOL 
NTAPI
PspManageSharedReadyQueue(UCHAR CoreNumber)
{
    PKCORE_SCHEDULER_DATA coreSchedulerData;
    INT checkedPriority;
    BOOL result;
    PKTHREAD thread;
    PLIST_ENTRY sharedReadyQueues, coreReadyQueues;

    result = FALSE;
    coreSchedulerData = &CoresSchedulerData[CoreNumber];

    sharedReadyQueues = PspSharedReadyQueue.ThreadReadyQueues;
    coreReadyQueues = coreSchedulerData->ThreadReadyQueues;

    /* if there are no threads in the shared queue, don't bother with locking it and just return */
    if (PspSharedReadyQueue.ThreadReadyQueuesSummary == 0)
        return FALSE;

    if (coreSchedulerData->ThreadReadyQueuesSummary >
        (PspSharedReadyQueue.ThreadReadyQueuesSummary ^ 
         coreSchedulerData->ThreadReadyQueuesSummary))
        return result;

    for (checkedPriority = 31; checkedPriority >= 0; checkedPriority--)
    {
        PLIST_ENTRY current;
            
        if (!TestBit(PspSharedReadyQueue.ThreadReadyQueuesSummary, checkedPriority))
            continue;

        current = sharedReadyQueues[checkedPriority].First;
                
        while (current != &sharedReadyQueues[checkedPriority])
        {
            thread = (PKTHREAD)((ULONG_PTR)current - FIELD_OFFSET(KTHREAD, ReadyQueueEntry));
                    
            /* check if this processor can even run this thread */
            if (thread->Affinity & (1LL << CoreNumber))
            {
                RemoveEntryList((PLIST_ENTRY)current);
                InsertTailList(&coreReadyQueues[checkedPriority], (PLIST_ENTRY)current);
                result = TRUE;

                SetSummaryBitIfNeccessary(coreReadyQueues, &coreSchedulerData->ThreadReadyQueuesSummary, checkedPriority);
                ClearSummaryBitIfNeccessary(sharedReadyQueues, &PspSharedReadyQueue.ThreadReadyQueuesSummary, checkedPriority);

                break;
            }

            current = current->Next;
        }
            

        if (result)
            break;
    }

    return result;
}

__declspec(noreturn) VOID PsExitThread(DWORD exitCode)
{
    PKTHREAD currentThread;
    KIRQL originalIrql;

    originalIrql = KiAcquireDispatcherLock();

    currentThread = KeGetCurrentThread();
    KeAcquireSpinLockAtDpcLevel(&currentThread->Header.Lock);
    KeAcquireSpinLockAtDpcLevel(&currentThread->ThreadLock);
    currentThread->ThreadState = THREAD_STATE_TERMINATED;
    currentThread->ThreadExitCode = exitCode;

    KiSignal((PDISPATCHER_HEADER)currentThread, -1);

    KeReleaseSpinLockFromDpcLevel(&currentThread->Header.Lock);
    KeReleaseSpinLockFromDpcLevel(&currentThread->ThreadLock);
    KeReleaseSpinLockFromDpcLevel(&DispatcherLock);

    HalDisableInterrupts();
    KeLowerIrql(originalIrql);
    KeForceClockTick();
    ASSERT(FALSE);
}

BOOL
NTAPI
KiSetUserMemory(
    PVOID Address,
    ULONG_PTR Data
)
{
    /* TODO: do checks before setting the data */
    *((ULONG_PTR*)Address) = Data;
    return TRUE;
}

/* TODO: implement a PspGetUsercallParameter maybe? */
VOID
PspSetUsercallParameter(
    PKTHREAD pThread,
    ULONG ParameterIndex,
    ULONG_PTR Value
)
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
        break;
    }
    }
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
    PVOID ReturnAddress
)
{
#ifdef _M_AMD64
    PKTASK_STATE pTaskState;
    INT i;

    pTaskState = (PKTASK_STATE)pThread->KernelStackPointer;

    /* allocate stack space for the registers that don't fit onto the stack */
    if (NumberOfParameters > 4)
    {
        pTaskState->Rsp -= (NumberOfParameters - 4) * sizeof(ULONG_PTR);
    }

    /* allocate the shadow space */
    pTaskState->Rsp -= 4 * sizeof(ULONG_PTR);

    /* allocate the return address */
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
    PKTASK_STATE TrapFrame
)
{
    if ((TrapFrame->Cs & 0x3) == 0x00)
        return KernelMode;
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
