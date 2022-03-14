#include "scheduler.h"
#include <pool.h>
#include <HAL/paging.h>
#include <SimpleTextIo.h>
#include <MemoryOperations.h>
#include <HAL/physical_allocator.h>
#include <HAL/pcr.h>
#include "ntqueue.h"
#include <bugcheck.h>
#include <nnxlog.h>
#include <HAL/mp.h>

KSPIN_LOCK ProcessListLock = 0;
LIST_ENTRY ProcessListHead;
KSPIN_LOCK ThreadListLock = 0;
LIST_ENTRY ThreadListHead;

UCHAR PspForegroundQuantum[3] = { 0x06, 0x0C, 0x12 };
UCHAR PspPrioritySeparation = 2;

KSPIN_LOCK DebugFramebufferSpinlock = 0;
UINT32* DebugFramebufferPosition;
SIZE_T DebugFramebufferLen = 16;

KSPIN_LOCK PrintLock = 0;
KSPIN_LOCK SchedulerCommonLock = 0;
BOOL gSchedulerInitialized = FALSE;

const CHAR PspThrePoolTag[4] = "Thre";
const CHAR PspProcPoolTag[4] = "Proc";

extern ULONG_PTR KeMaximumIncrement;
extern ULONG_PTR KiCyclesPerClockQuantum;
extern ULONG_PTR KiCyclesPerQuantum;
extern UINT KeNumberOfProcessors;
UINT PspCoresInitialized = 0;

ULONG_PTR PspCreateKernelStack();
VOID PspSetupThreadState(PKTASK_STATE pThreadState, BOOL IsKernel, ULONG_PTR EntryPoint, ULONG_PTR Userstack);
VOID HalpUpdateThreadKernelStack(PVOID kernelStack);
NTSTATUS PspCreateProcessInternal(PEPROCESS* ppProcess);
NTSTATUS PspCreateThreadInternal(PETHREAD* ppThread, PEPROCESS pParentProcess, BOOL IsKernel, ULONG_PTR EntryPoint);
NTSTATUS PspDestroyProcessInternal(PEPROCESS pProcess);
NTSTATUS PspDestroyThreadInternal(PETHREAD pThread);
VOID PspInsertIntoSharedQueue(PKTHREAD Thread);
__declspec(noreturn) VOID PspTestAsmUser();
__declspec(noreturn) VOID PspTestAsmUserEnd();
__declspec(noreturn) VOID PspTestAsmUser2();
__declspec(noreturn) VOID PspIdleThreadProcedure();

BOOL TestBit(ULONG_PTR Number, ULONG_PTR BitIndex);
ULONG_PTR ClearBit(ULONG_PTR Number, ULONG_PTR BitIndex);
ULONG_PTR SetBit(ULONG_PTR Number, ULONG_PTR BitIndex);

struct _READY_QUEUES
{
    /* a ready queue for each thread priority */
    KQUEUE ThreadReadyQueues[32];
    ULONG ThreadReadyQueuesSummary;
};

typedef struct _KPROCESSOR_READY_QUEUES
{
    struct _READY_QUEUES;
    PEPROCESS IdleProcess;
    PETHREAD IdleThread;
}KCORE_SCHEDULER_DATA, *PKCORE_SCHEDULER_DATA;

typedef struct _KSHARED_READY_QUEUE
{
    struct _READY_QUEUES;
    KSPIN_LOCK Lock;
}KSHARED_READY_QUEUE, *PKSHARED_READY_QUEUE;

PKCORE_SCHEDULER_DATA   CoresSchedulerData = (PKCORE_SCHEDULER_DATA)NULL;
KSHARED_READY_QUEUE     PspSharedReadyQueue = { 0 };

/* clear the summary bit for this priority if there are none entries left */
inline VOID ClearSummaryBitIfNeccessary(PKQUEUE ThreadReadyQueues, PULONG Summary, UCHAR Priority)
{
    if (IsListEmpty(&ThreadReadyQueues[Priority].EntryListHead))
    {
        *Summary = (ULONG)ClearBit(
            *Summary,
            Priority
        );
    }
}

inline VOID SetSummaryBitIfNeccessary(PKQUEUE ThreadReadyQueues, PULONG Summary, UCHAR Priority)
{
    if (!IsListEmpty(&ThreadReadyQueues[Priority].EntryListHead))
    {
        *Summary = (ULONG)SetBit(
            *Summary,
            Priority
        );
    }
}

NTSTATUS PspInitializeScheduler()
{
    INT i;
    KIRQL irql;

    KeInitializeSpinLock(&SchedulerCommonLock);
    KeAcquireSpinLock(&SchedulerCommonLock, &irql);
    if (CoresSchedulerData == NULL)
    {
        KeInitializeSpinLock(&ProcessListLock);
        KeInitializeSpinLock(&ThreadListLock);
        KeInitializeSpinLock(&PspSharedReadyQueue.Lock);

        InitializeListHead(&ThreadListHead);
        InitializeListHead(&ProcessListHead);

        CoresSchedulerData = (PKCORE_SCHEDULER_DATA)PagingAllocatePageBlockFromRange(
            (KeNumberOfProcessors * sizeof(KCORE_SCHEDULER_DATA) + PAGE_SIZE - 1) / PAGE_SIZE,
            PAGING_KERNEL_SPACE,
            PAGING_KERNEL_SPACE_END
        );

        if (CoresSchedulerData != NULL)
        {
            KeAcquireSpinLockAtDpcLevel(&PspSharedReadyQueue.Lock);
            for (i = 0; i < 32; i++)
            {
                KeInitializeQueue(&PspSharedReadyQueue.ThreadReadyQueues[i], 0);
            }
            KeReleaseSpinLockFromDpcLevel(&PspSharedReadyQueue.Lock);
        }
    }
    KeReleaseSpinLock(&SchedulerCommonLock, irql);
    return (CoresSchedulerData == (PKCORE_SCHEDULER_DATA)NULL) ? (STATUS_NO_MEMORY) : (STATUS_SUCCESS);
}

PKTHREAD PspSelectNextReadyThread(UCHAR CoreNumber)
{
    INT priority;
    PKTHREAD result;
    PKCORE_SCHEDULER_DATA coreSchedulerData = &CoresSchedulerData[CoreNumber];

    PspManageSharedReadyQueue(CoreNumber);

    result = &CoresSchedulerData->IdleThread->Tcb;

    /* start with the highest priority */
    for (priority = 31; priority >= 0; priority--)
    {
        if (TestBit(coreSchedulerData->ThreadReadyQueuesSummary, priority))
        {
            PLIST_ENTRY_POINTER dequeuedEntry = (PLIST_ENTRY_POINTER)RemoveHeadList(&coreSchedulerData->ThreadReadyQueues[priority].EntryListHead);
            result = dequeuedEntry->Pointer;

            ClearSummaryBitIfNeccessary(coreSchedulerData->ThreadReadyQueues, &coreSchedulerData->ThreadReadyQueuesSummary, priority);
            break;
        }
    }

    /* if no process was found on queue(s), return the idle thread */
    return result;
}

static VOID Test()
{
    const UINT32 coreColors[] = {0xff0000ff, 0xff00ff00, 0xff00ffff, 0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff};
    UINT8 coreNumber = KeGetCurrentProcessorId();
    UINT32 coreColor = coreColors[coreNumber];
    UINT32 clearColor = 0x00000000;
    const SIZE_T bufferHeight = 16, bufferWidth = 16;

    SIZE_T bufferPositionAbsolute = coreNumber;
    SIZE_T bufferPositionGridX = bufferPositionAbsolute % (gWidth / bufferWidth);
    SIZE_T bufferPositionGridY = bufferPositionAbsolute / (gWidth / bufferWidth);
    SIZE_T bufferPositionX = bufferPositionGridX * bufferWidth;
    SIZE_T bufferPositionY = bufferPositionGridY * bufferHeight;

    SIZE_T i, j;

    while (TRUE)
    {
        for (i = 0; i < bufferWidth; i++)
        {
            for (j = 0; j < bufferHeight; j++)
            {
                UINT32 temp;
                gFramebuffer[(i + bufferPositionX) + (j + bufferPositionY) * gPixelsPerScanline] = coreColor;

                temp = coreColor;
                coreColor = clearColor;
                clearColor = temp;
            }
        }
    }
}

VOID Test1()
{
    while (1)
    {
        if (KeGetCurrentProcessorId() == 0)
        {
            for (unsigned x = gWidth * gHeight; x > 0 ; x--)
            {
                for (int i = 0; i < 10000; i++);
                gFramebuffer[x] = 0x00FF00FF;
            }
        }
    }
}

VOID Test2()
{
    while (1)
    {
        if (KeGetCurrentProcessorId() == 0)
        {
            for (unsigned x = 0; x < gWidth * gHeight; x++)
            {
                for (int i = 0; i < 10000; i++);
                gFramebuffer[x] = 0xFF00FF00;
            }
        }
    }
}

NTSTATUS PspTestScheduler(PEPROCESS IdleProcess)
{
    PETHREAD test1, test2;
    NTSTATUS status;

    status = PspCreateThreadInternal(&test1, IdleProcess, TRUE, (ULONG_PTR)Test1);
    if (status)
        return status;

    status = PspCreateThreadInternal(&test2, IdleProcess, TRUE, (ULONG_PTR)Test2);
    if (status)
        return status;

    test1->Tcb.ThreadPriority = 1;
    test2->Tcb.ThreadPriority = 2;

    PspInsertIntoSharedQueue(&test1->Tcb);
    PspInsertIntoSharedQueue(&test2->Tcb);

    return status;
}

NTSTATUS PspCreateIdleProcessForCore(PEPROCESS* IdleProcess, PETHREAD* IdleThread, UINT8 coreNumber) 
{
    NTSTATUS status;

    status = PspCreateProcessInternal(IdleProcess);
    if (status)
        return status;

    (*IdleProcess)->Pcb.AffinityMask = (1ULL << (ULONG_PTR)coreNumber);

    status = PspCreateThreadInternal(IdleThread, *IdleProcess, TRUE, (ULONG_PTR)PspIdleThreadProcedure);
    if (status)
        return status;

    (*IdleThread)->Tcb.ThreadPriority = 0;

    /* uncomment the lines below to do some basic scheduler testing */
    /*
    if (coreNumber == 0)
        PspTestScheduler(*IdleProcess);
    */

    return STATUS_SUCCESS;
}

VOID PspInitializeCoreSchedulerData(UINT8 CoreNumber)
{
    INT i;
    PKCORE_SCHEDULER_DATA thiscoreSchedulerData = &CoresSchedulerData[CoreNumber];
    NTSTATUS status;

    thiscoreSchedulerData->ThreadReadyQueuesSummary = 0;

    for (i = 0; i < 32; i++)
    {
        KeInitializeQueue(&thiscoreSchedulerData->ThreadReadyQueues[i], 0);
    }

    status = PspCreateIdleProcessForCore(&thiscoreSchedulerData->IdleProcess, &thiscoreSchedulerData->IdleThread, CoreNumber);
    if (status != STATUS_SUCCESS)
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, status, 0, 0, 0);
}


#pragma warning(push)
NTSTATUS PspInitializeCore(UINT8 CoreNumber)
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
        
/* don't care about lock releasing, the system is dead anyway if it returns here */
#pragma warning(disable: 26115)
        if (status)
            return status;
    }

    KeAcquireSpinLock(&SchedulerCommonLock, &irql);
    PspInitializeCoreSchedulerData(CoreNumber);

    pPcr = KeGetPcr();
    pPrcb = pPcr->Prcb;
    KeAcquireSpinLockAtDpcLevel(&pPrcb->Lock);
#pragma warning(disable: 6011)
    pPrcb->IdleThread = &CoresSchedulerData[CoreNumber].IdleThread->Tcb;
    pPrcb->NextThread = pPrcb->IdleThread;
    pPrcb->CurrentThread = pPrcb->IdleThread;
    pPrcb->CpuCyclesRemaining = 0;
    KeReleaseSpinLockFromDpcLevel(&pPrcb->Lock);

    PspCoresInitialized++;
    KeReleaseSpinLock(&SchedulerCommonLock, irql);
    
    HalpUpdateThreadKernelStack((PVOID)((ULONG_PTR)pPrcb->IdleThread->KernelStackPointer + sizeof(KTASK_STATE)));
    PagingSetAddressSpace(pPrcb->IdleThread->Process->AddressSpacePhysicalPointer);
    pPrcb->IdleThread->ThreadState = THREAD_STATE_RUNNING;
    PspSwitchContextTo64(pPrcb->IdleThread->KernelStackPointer);

    return STATUS_SUCCESS;
}
#pragma warning(pop)

ULONG_PTR PspScheduleThread(ULONG_PTR stack)
{
    PKPCR pcr;
    KIRQL irql;

    PKSPIN_LOCK originalRunningThreadSpinlock, originalRunningProcessSpinlock;
    PKTHREAD nextThread;
    PKTHREAD originalRunningThread;
    PKPROCESS originalRunningProcess;
    UCHAR originalRunningThreadPriority;

    pcr = KeGetPcr();

    KeAcquireSpinLock(&ProcessListLock, &irql);
    KeAcquireSpinLockAtDpcLevel(&pcr->Prcb->Lock);

    originalRunningThread = pcr->Prcb->CurrentThread;
    if (originalRunningThread == NULL)
    {
        KeBugCheckEx(WORKER_THREAD_TEST_CONDITION, (ULONG_PTR)originalRunningThread, 0, 0, 0);
    }

    originalRunningThreadSpinlock = &pcr->Prcb->CurrentThread->ThreadLock;
    KeAcquireSpinLockAtDpcLevel(originalRunningThreadSpinlock);
    
    originalRunningProcess = originalRunningThread->Process;
    if (originalRunningProcess == NULL)
    {
        KeBugCheckEx(WORKER_THREAD_TEST_CONDITION, (ULONG_PTR)originalRunningThread, 0, 0, 0);
    }
    originalRunningProcessSpinlock = &originalRunningProcess->ProcessLock;
    KeAcquireSpinLockAtDpcLevel(originalRunningProcessSpinlock);

    originalRunningThreadPriority = originalRunningThread->ThreadPriority + originalRunningProcess->BasePriority;

    if (pcr->Prcb->IdleThread == pcr->Prcb->NextThread)
    {
        pcr->Prcb->NextThread = PspSelectNextReadyThread(pcr->Prcb->Number);
    }

    /* 
        if no next thread was found, reset the quantum for current thread
        no need to check queues, that should be dealt with scheduler events
        (thread of higher priority than NextThread would automaticaly be
        written to NextThread on for example end of waiting) 
    */
    if ((pcr->Prcb->NextThread == pcr->Prcb->IdleThread && pcr->Prcb->CurrentThread != pcr->Prcb->IdleThread) ||
        (originalRunningThreadPriority > pcr->Prcb->NextThread->ThreadPriority + pcr->Prcb->NextThread->Process->BasePriority))
    {
        pcr->Prcb->NextThread = originalRunningThread;
        pcr->CyclesLeft = originalRunningProcess->QuantumReset * KiCyclesPerQuantum;
    }

    if (pcr->CyclesLeft < (LONG_PTR)KiCyclesPerQuantum)
    {
        /* switch to next thread */
        nextThread = pcr->Prcb->NextThread;
        pcr->CyclesLeft = nextThread->Process->QuantumReset * KiCyclesPerQuantum;

        /* if thread was found */
        if (nextThread != originalRunningThread)
        {
            if (pcr->Prcb->CurrentThread->Process != pcr->Prcb->NextThread->Process)
            {
                PagingSetAddressSpace((ULONG_PTR)pcr->Prcb->CurrentThread->Process->AddressSpacePhysicalPointer);
            }
        }

        nextThread->ThreadState = THREAD_STATE_RUNNING;
        
        /* select thread before setting the original one to ready */
        pcr->Prcb->NextThread = PspSelectNextReadyThread(pcr->Prcb->Number);
        pcr->Prcb->CurrentThread = nextThread;

        /*
            thread could have volountarily given up control, it could be waiting - then its state shouldn't be changed
        */
        if (originalRunningThread->ThreadState == THREAD_STATE_RUNNING)
        {
            PspInsertIntoSharedQueue(originalRunningThread);
            originalRunningThread->ThreadState = THREAD_STATE_READY;
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
    KeReleaseSpinLockFromDpcLevel(originalRunningThreadSpinlock);
    KeReleaseSpinLockFromDpcLevel(&pcr->Prcb->Lock);
    KeReleaseSpinLock(&ProcessListLock, irql);

    return (ULONG_PTR)pcr->Prcb->CurrentThread->KernelStackPointer;
}

PKTHREAD PspGetCurrentThread()
{
    PKPCR pcr = KeGetPcr();
    return pcr->Prcb->CurrentThread;
}

PKTHREAD KeGetCurrentThread()
{
    return PspGetCurrentThread();
}

ULONG_PTR PspCreateKernelStack()
{
    return (ULONG_PTR)PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END) + PAGE_SIZE;
}

VOID PspSetupThreadState(PKTASK_STATE pThreadState, BOOL IsKernel, ULONG_PTR Entrypoint, ULONG_PTR Userstack)
{
    MemSet(pThreadState, 0, sizeof(*pThreadState));
    pThreadState->Rip = (UINT64)Entrypoint;
    pThreadState->Cs = IsKernel ? 0x08 : 0x1B;
    pThreadState->Ds = IsKernel ? 0x10 : 0x23;
    pThreadState->Es = IsKernel ? 0x10 : 0x23;
    pThreadState->Fs = IsKernel ? 0x10 : 0x23;
    pThreadState->Gs = IsKernel ? 0x10 : 0x23;
    pThreadState->Ss = IsKernel ? 0x10 : 0x23;
    pThreadState->Rflags = 0x00000286;
    pThreadState->Rsp = Userstack;
}

NTSTATUS PspCreateProcessInternal(PEPROCESS* ppProcess)
{
    PEPROCESS pProcess;
    KIRQL irql;
    PLIST_ENTRY_POINTER processEntry;

    pProcess = ExAllocatePoolWithTag(NonPagedPool, sizeof(EPROCESS), POOL_TAG_FROM_STR(PspProcPoolTag));
    if (pProcess == NULL)
        return STATUS_NO_MEMORY;

    processEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(*processEntry), POOL_TAG_FROM_STR(PspProcPoolTag));
    if (processEntry == NULL)
    {
        ExFreePool(pProcess);
        return STATUS_NO_MEMORY;
    }

    /* lock the process list */
    KeAcquireSpinLock(&ProcessListLock, &irql);

    processEntry->Pointer = pProcess;

    /* make sure it is not prematurely used */
    pProcess->Initialized = FALSE;
    KeInitializeSpinLock(&pProcess->Pcb.ProcessLock);
    
    /* lock the process */
    KeAcquireSpinLock(&pProcess->Pcb.ProcessLock, &irql);

    InitializeDispatcherHeader(&pProcess->Pcb.Header, OBJECT_TYPE_KPROCESS);
    KeInitializeSpinLock(&pProcess->Pcb.ProcessLock);
    InitializeListHead(&pProcess->Pcb.ThreadListHead);
    pProcess->Pcb.BasePriority = 0;
    pProcess->Pcb.AffinityMask = KAFFINITY_ALL;
    pProcess->Pcb.NumberOfThreads = 0;
    pProcess->Pcb.AddressSpacePhysicalPointer = PagingCreateAddressSpace();
    pProcess->Pcb.QuantumReset = 6;
    InitializeListHead(&pProcess->Pcb.HandleDatabaseHead);

    InsertTailList(&ProcessListHead, (PLIST_ENTRY)processEntry);

    KeReleaseSpinLockFromDpcLevel(&pProcess->Pcb.ProcessLock);
    KeReleaseSpinLock(&ProcessListLock, irql);

    *ppProcess = pProcess;

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
NTSTATUS PspCreateThreadInternal(PETHREAD* ppThread, PEPROCESS pParentProcess, BOOL IsKernel, ULONG_PTR EntryPoint)
{
    KIRQL irql;
    PETHREAD pThread;
    PLIST_ENTRY_POINTER threadListEntry, threadListInProcessEntry;
    ULONG_PTR userstack;
    ULONG_PTR originalAddressSpace;

    pThread = ExAllocatePoolWithTag(NonPagedPool, sizeof(*pThread), POOL_TAG_FROM_STR(PspThrePoolTag));
    if (pThread == NULL)
    {
        return STATUS_NO_MEMORY;
    }

    threadListEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(LIST_ENTRY_POINTER), POOL_TAG_FROM_STR(PspThrePoolTag));
    if (threadListEntry == NULL)
    {
        ExFreePool(pThread);
        return STATUS_NO_MEMORY;
    }

    threadListInProcessEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(LIST_ENTRY_POINTER), POOL_TAG_FROM_STR(PspThrePoolTag));
    if ((PVOID)threadListInProcessEntry == NULL)
    {
        ExFreePool(pThread);
        ExFreePool(threadListEntry);
        return STATUS_NO_MEMORY;
    }
    
    KeAcquireSpinLock(&ThreadListLock, &irql);

    threadListInProcessEntry->Pointer = &pThread->Tcb;
    threadListEntry->Pointer = pThread;

    pThread->Tcb.ThreadState = THREAD_STATE_INITIALIZATION;
    KeInitializeSpinLock(&pThread->Tcb.ThreadLock);
    
    KeAcquireSpinLockAtDpcLevel(&pThread->Tcb.ThreadLock);
    pThread->Process = pParentProcess;
    pThread->StartAddress = 0;

    KeAcquireSpinLockAtDpcLevel(&pThread->Process->Pcb.ProcessLock);

    originalAddressSpace = PagingGetAddressSpace();
    PagingSetAddressSpace(pThread->Process->Pcb.AddressSpacePhysicalPointer);

    userstack = (ULONG_PTR)PagingAllocatePageFromRange(PAGING_USER_SPACE, PAGING_USER_SPACE_END) + (ULONG_PTR)PAGE_SIZE;

    PagingSetAddressSpace(originalAddressSpace);

    InitializeDispatcherHeader(&pThread->Tcb.Header, OBJECT_TYPE_KTHREAD);
    InitializeListHead((PLIST_ENTRY)&pThread->Tcb.WaitHead);
    MemSet(pThread->Tcb.ThreadWaitBlocks, 0, sizeof(pThread->Tcb.ThreadWaitBlocks));
    pThread->Tcb.ThreadPriority = 0;
    pThread->Tcb.NumberOfCustomThreadWaitBlocks = 0;
    pThread->Tcb.CustomThreadWaitBlocks = (PKWAIT_BLOCK)NULL;
    pThread->Tcb.WaitStatus = 0;
    pThread->Tcb.Alertable = FALSE;
    pThread->Tcb.Process = &pParentProcess->Pcb;
    pThread->Tcb.Timeout = 0;
    pThread->Tcb.TimeoutIsAbsolute = FALSE;
    pThread->Tcb.KernelStackPointer = (PVOID)(PspCreateKernelStack() - sizeof(KTASK_STATE));
    /* inherit affinity after the parent process */
    pThread->Tcb.Affinity = pThread->Tcb.Process->AffinityMask;
    PspSetupThreadState((PKTASK_STATE)pThread->Tcb.KernelStackPointer, IsKernel, EntryPoint, userstack);

    InsertTailList(&pParentProcess->Pcb.ThreadListHead, (PLIST_ENTRY)threadListInProcessEntry);
    KeReleaseSpinLockFromDpcLevel(&pThread->Process->Pcb.ProcessLock);

    InsertTailList(&ThreadListHead, (PLIST_ENTRY)threadListEntry);
    
    pThread->Tcb.ThreadState = THREAD_STATE_READY;

    KeReleaseSpinLockFromDpcLevel(&pThread->Tcb.ThreadLock);
    KeReleaseSpinLock(&ThreadListLock, irql);
    *ppThread = pThread;


    return STATUS_SUCCESS;
}

/** @brief this version of PspDestoryThreadInternal is needed so PspDestroyProcessInternal doesn't deadlock on trying to destory its threads 
TODO: free auto-allocated user stack (somehow, maybe store the original stack in KTHREAD?) */
NTSTATUS PspDestroyThreadInternalOptionalProcessLock(PETHREAD pThread, BOOL LockParent)
{
    KIRQL irql;
    PLIST_ENTRY_POINTER current;
    PLIST_ENTRY_POINTER threadListEntry, threadListInProcessEntry;

    threadListEntry = (PLIST_ENTRY_POINTER)NULL;
    threadListInProcessEntry = (PLIST_ENTRY_POINTER)NULL;

    KeAcquireSpinLockAtDpcLevel(&pThread->Tcb.ThreadLock);
    KeAcquireSpinLock(&ThreadListLock, &irql);
    pThread->Tcb.ThreadState = THREAD_STATE_TERMINATED;

    if (LockParent)
        KeAcquireSpinLockAtDpcLevel(&pThread->Process->Pcb.ProcessLock);
    
    current = (PLIST_ENTRY_POINTER)pThread->Process->Pcb.ThreadListHead.First;
    while ((PLIST_ENTRY)current != &pThread->Process->Pcb.ThreadListHead)
    {
        if (current->Pointer == &pThread->Tcb)
        {
            threadListInProcessEntry = current;
            break;
        }

        current = (PLIST_ENTRY_POINTER)current->Next;
    }

    if ((PVOID)threadListInProcessEntry == NULL)
    {
        if (LockParent)
            KeReleaseSpinLockFromDpcLevel(&pThread->Process->Pcb.ProcessLock);
        KeReleaseSpinLockFromDpcLevel(&pThread->Tcb.ThreadLock);
        KeReleaseSpinLock(&ThreadListLock, irql);
        return STATUS_INVALID_PARAMETER;
    }

    RemoveEntryList((PLIST_ENTRY)threadListInProcessEntry);
    ExFreePool(threadListInProcessEntry);
    if (LockParent)
        KeReleaseSpinLockFromDpcLevel(&pThread->Process->Pcb.ProcessLock);

    
    current = (PLIST_ENTRY_POINTER)ThreadListHead.First;
    while (current != (PLIST_ENTRY_POINTER)&ThreadListHead)
    {
        if (current->Pointer == pThread)
        {
            threadListEntry = current;
            break;
        }

        current = (PLIST_ENTRY_POINTER)current->Next;
    }

    if ((PVOID)threadListEntry == NULL)
    {
        KeReleaseSpinLockFromDpcLevel(&pThread->Tcb.ThreadLock);
        KeReleaseSpinLock(&ThreadListLock, irql);
        return STATUS_INVALID_PARAMETER;
    }

    RemoveEntryList((PLIST_ENTRY)threadListEntry);
    ExFreePool(threadListEntry);

    KeReleaseSpinLockFromDpcLevel(&pThread->Tcb.ThreadLock);
    KeReleaseSpinLock(&ThreadListLock, irql);
    ExFreePool(pThread);

    return STATUS_SUCCESS;
}

NTSTATUS PspDestroyThreadInternal(PETHREAD Thread)
{
    return PspDestroyThreadInternalOptionalProcessLock(Thread, TRUE);
}

/**
 * @brief removes pProcess from process list, destroys process' the handle database, 
 * and if that is successful, performs ExFreePool
 * @param pProcess - pointer to EPROCESS to be terminated 
 * @return STATUS_SUCCESS, STATUS_INVALID_PARAMETER
*/
NTSTATUS PspDestroyProcessInternal(PEPROCESS pProcess)
{
    KIRQL irql;
    PLIST_ENTRY_POINTER processListEntry;
    PLIST_ENTRY_POINTER current;
    PHANDLE_DATABASE currentHandleDatabase;

    KeAcquireSpinLock(&ProcessListLock, &irql);

    KeAcquireSpinLockAtDpcLevel(&pProcess->Pcb.ProcessLock);
    
    current = (PLIST_ENTRY_POINTER)pProcess->Pcb.ThreadListHead.First;
    while (current != (PLIST_ENTRY_POINTER)&pProcess->Pcb.ThreadListHead)
    {
        PETHREAD Thread;
        Thread = (PETHREAD)current->Pointer;
        current = (PLIST_ENTRY_POINTER)current->Next;
        
        /* don't lock the process, it has been already locked */
        PspDestroyThreadInternalOptionalProcessLock(Thread, FALSE);
    }

    currentHandleDatabase = (PHANDLE_DATABASE) pProcess->Pcb.HandleDatabaseHead.First;
    while (currentHandleDatabase != (PHANDLE_DATABASE)&pProcess->Pcb.HandleDatabaseHead)
    {
        PHANDLE_DATABASE next;
        INT i;

        next = (PHANDLE_DATABASE)currentHandleDatabase->HandleDatabaseChainEntry.Next;

        for (i = 0; i < ENTRIES_PER_HANDLE_DATABASE; i++)
        {
            ObDestroyHandleEntry(&currentHandleDatabase->Entries[i]);
        }

        ExFreePool(currentHandleDatabase);
        currentHandleDatabase = next;
    }

    processListEntry = FindElementInPointerList((PLIST_ENTRY_POINTER)&ProcessListHead, (PVOID)pProcess);

    if ((PVOID)processListEntry == NULL)
    {
        KeReleaseSpinLockFromDpcLevel(&pProcess->Pcb.ProcessLock);
        KeReleaseSpinLock(&ProcessListLock, irql);
        return STATUS_INVALID_PARAMETER;
    }
    else
    {
        RemoveEntryList((PLIST_ENTRY)processListEntry);
    }

    InternalFreePhysicalPage(pProcess->Pcb.AddressSpacePhysicalPointer);

    KeReleaseSpinLockFromDpcLevel(&pProcess->Pcb.ProcessLock);
    KeReleaseSpinLock(&ProcessListLock, irql);

    ExFreePool(pProcess);
    return STATUS_SUCCESS;
}

VOID PspInsertIntoSharedQueue(PKTHREAD Thread)
{
    UCHAR ThreadPriority;
    KIRQL irql;

    KeAcquireSpinLock(&PspSharedReadyQueue.Lock, &irql);

    ThreadPriority = (UCHAR)(Thread->ThreadPriority + (CHAR)Thread->Process->BasePriority);
    Thread->ReadyQueueEntry.Pointer = Thread;
    InsertTailList(&PspSharedReadyQueue.ThreadReadyQueues[ThreadPriority].EntryListHead, (PLIST_ENTRY)&Thread->ReadyQueueEntry);
    PspSharedReadyQueue.ThreadReadyQueuesSummary = (ULONG)SetBit(
        PspSharedReadyQueue.ThreadReadyQueuesSummary,
        ThreadPriority
    );

    KeReleaseSpinLock(&PspSharedReadyQueue.Lock, irql);
}

BOOL PsCheckThreadIsReady(PKTHREAD Thread)
{
    KIRQL irql;
    BOOL ready;

    KeAcquireSpinLock(&Thread->ThreadLock, &irql);

    if (IsListEmpty((PLIST_ENTRY)&Thread->WaitHead))
    {
        Thread->Timeout = 0;
        Thread->TimeoutIsAbsolute = 0;
        if (Thread->NumberOfCustomThreadWaitBlocks)
        {
            Thread->NumberOfCustomThreadWaitBlocks = 0;
            Thread->CustomThreadWaitBlocks = 0;
        }
        Thread->ThreadState = THREAD_STATE_READY;

        PspInsertIntoSharedQueue(Thread);
    }

    /* 
        don't just set it to FALSE at the init and later change it to TRUE in the if block, 
        wouldn't work on a thread that's already ready 
    */
    ready = (Thread->ThreadState == THREAD_STATE_READY);

    KeReleaseSpinLock(&Thread->ThreadLock, irql);

    return ready;
}

BOOL PspManageSharedReadyQueue(UCHAR CoreNumber)
{
    PKCORE_SCHEDULER_DATA coreSchedulerData;
    KIRQL irql;
    INT checkedPriority;
    BOOL result;
    PKTHREAD thread;
    PKQUEUE sharedReadyQueues, coreReadyQueues;

    result = FALSE;
    coreSchedulerData = &CoresSchedulerData[CoreNumber];

    sharedReadyQueues = PspSharedReadyQueue.ThreadReadyQueues;
    coreReadyQueues = coreSchedulerData->ThreadReadyQueues;

    /* if there are no threads in the shared queue, don't bother with locking it and just return */
    if (PspSharedReadyQueue.ThreadReadyQueuesSummary == 0)
        return FALSE;

    KeAcquireSpinLock(&PspSharedReadyQueue.Lock, &irql);
    /* ugly 5 level nesting, but it works... or so i hope */

    /* 
        the XOR between summaries gives us the bits in them that are different
        if those bits are greater than coreSchedulerData's ready queue, it means 
        that there was a higher bit in PspSharedReadyQueue's summary, which means
        it has a thread with a higher priority ready
    */
    if (coreSchedulerData->ThreadReadyQueuesSummary <= (PspSharedReadyQueue.ThreadReadyQueuesSummary ^ coreSchedulerData->ThreadReadyQueuesSummary))
    {
        
        for (checkedPriority = 31; checkedPriority >= 0; checkedPriority--)
        {
            
            if (TestBit(PspSharedReadyQueue.ThreadReadyQueuesSummary, checkedPriority))
            {
                PLIST_ENTRY_POINTER current = (PLIST_ENTRY_POINTER)sharedReadyQueues[checkedPriority].EntryListHead.First;
                
                while (current != (PLIST_ENTRY_POINTER)&sharedReadyQueues[checkedPriority].EntryListHead)
                {
                    thread = (PKTHREAD)current->Pointer;
                    
                    /* check if this processor can even run this thread */
                    if (thread->Affinity & (1LL << CoreNumber))
                    {
                        RemoveEntryList((PLIST_ENTRY)current);
                        InsertTailList(&coreReadyQueues[checkedPriority].EntryListHead, (PLIST_ENTRY)current);
                        result = TRUE;

                        SetSummaryBitIfNeccessary(coreReadyQueues, &coreSchedulerData->ThreadReadyQueuesSummary, checkedPriority);
                        ClearSummaryBitIfNeccessary(sharedReadyQueues, &PspSharedReadyQueue.ThreadReadyQueuesSummary, checkedPriority);

                        break;
                    }

                    current = (PLIST_ENTRY_POINTER)current->Next;
                }
            }

            if (result)
                break;
        }
    }

    KeReleaseSpinLock(&PspSharedReadyQueue.Lock, irql);

    return result;
}
