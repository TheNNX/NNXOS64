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
#include <HAL/cpu.h>
#include <ob/object.h>

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

POBJECT_TYPE PsProcessType = NULL;
POBJECT_TYPE PsThreadType = NULL;

NTSTATUS PspInitializeScheduler()
{
    INT i;
    KIRQL irql;
    NTSTATUS status;

    KeInitializeSpinLock(&SchedulerCommonLock);
    KeAcquireSpinLock(&SchedulerCommonLock, &irql);

    status = ObCreateSchedulerTypes(
        &PsProcessType, 
        &PsThreadType
    );

    if (status != STATUS_SUCCESS)
    {
        KeReleaseSpinLock(&SchedulerCommonLock, irql);
        return status;
    }

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
    PKCORE_SCHEDULER_DATA coreOwnData = &CoresSchedulerData[CoreNumber];

    PspManageSharedReadyQueue(CoreNumber);
    result = &coreOwnData->IdleThread->Tcb;

    /* start with the highest priority */
    for (priority = 31; priority >= 0; priority--)
    {
        if (TestBit(coreOwnData->ThreadReadyQueuesSummary, priority))
        {
            PLIST_ENTRY dequeuedEntry = (PLIST_ENTRY)RemoveHeadList(&coreOwnData->ThreadReadyQueues[priority].EntryListHead);
            result = (PKTHREAD)((ULONG_PTR)dequeuedEntry - FIELD_OFFSET(KTHREAD, ReadyQueueEntry));

            ClearSummaryBitIfNeccessary(coreOwnData->ThreadReadyQueues, &coreOwnData->ThreadReadyQueuesSummary, priority);
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
        for (unsigned x = gWidth * gHeight; x > 0 ; x--)
        {
            for (int i = 0; i < 49999; i++);
            gFramebuffer[x] = 0x00FF00FF;
        }
    }
}

VOID Test2()
{
    while (1)
    {
        for (unsigned x = 0; x < gWidth * gHeight; x++)
        {
            for (int i = 0; i < 99999; i++);
            gFramebuffer[x] = 0xFF00FF00;
        }
    }
}

INT32 exits = 0;
VOID DiagnosticThread()
{
    KIRQL irql;
    PKPCR pcr;

    pcr = KeGetPcr();

    while (1)
    {
        INT i;

        for (i = 0; i < 2999999; i++);

        KeAcquireSpinLock(&ProcessListLock, &irql);
        KeAcquireSpinLockAtDpcLevel(&pcr->Prcb->Lock);

        const UINT32 minX = gWidth * 7 / 16;
        const UINT32 maxX = gWidth * 15 / 16;
        const UINT32 minY = 100;
        const UINT32 maxY = 400;

        const UINT32 controlColor = 0xFFC3C3C3, controlBckgrd = 0xFF606060;
        const UINT32 usedColor = 0xFF800000, freeColor = 0xFF008000;
        const UINT32 pad = 10;
        UINT32* cFramebuffer = gFramebuffer + minY * gPixelsPerScanline;
        UINT32 oldBoundingBox[4], newBoundingBox[4] = { minX, maxX, minY, maxY };
        UINT32 cursorX, cursorY, oldColor, oldBackground, currentPosX, currentPosY;
        UINT8 oldRenderBack;

        for (UINT32 y = minY; y < maxY; y++)
        {
            cFramebuffer += gPixelsPerScanline;
            for (UINT32 x = minX; x < maxX; x++)
            {
                cFramebuffer[x] = controlColor;
            }
        }

        TextIoGetCursorPosition(&cursorX, &cursorY);
        TextIoGetBoundingBox(oldBoundingBox);
        TextIoSetBoundingBox(newBoundingBox);
        TextIoGetColorInformation(&oldColor, &oldBackground, &oldRenderBack);

        TextIoSetCursorPosition(minX, minY + 6);
        TextIoSetColorInformation(0xFF000000, controlColor, 0);

        KeAcquireSpinLockAtDpcLevel(&PspSharedReadyQueue.Lock);
        PrintT("Debug thread:\n\nSummaries:\n");

        PrintT("%b\n\nQueue sizes:",PspSharedReadyQueue.ThreadReadyQueuesSummary);
        
        for (i = 0; i < 32; i++)
        {
            int count = 0;
            PLIST_ENTRY currentEntry = PspSharedReadyQueue.ThreadReadyQueues[i].EntryListHead.First;
            while (currentEntry != &PspSharedReadyQueue.ThreadReadyQueues[i].EntryListHead)
            {
                count++;
                currentEntry = currentEntry->Next;
            }

            PrintT(i == 31 ? "%i\n\n" : "%i, ", count);
        }

        for (i = 0; i < 32; i++)
        {
            PLIST_ENTRY currentEntry = PspSharedReadyQueue.ThreadReadyQueues[i].EntryListHead.First;
            while (currentEntry != &PspSharedReadyQueue.ThreadReadyQueues[i].EntryListHead)
            {
                PKTHREAD thread = (PKTHREAD)((ULONG_PTR)currentEntry - FIELD_OFFSET(KTHREAD, ReadyQueueEntry));
                PrintT("Pr%i %X: state %i\n", i, thread, thread->ThreadState);
                currentEntry = currentEntry->Next;
            }
        }
        
        KeReleaseSpinLockFromDpcLevel(&PspSharedReadyQueue.Lock);

        TextIoGetCursorPosition(&currentPosX, &currentPosY);

        cFramebuffer = gFramebuffer + (currentPosX + pad) + currentPosY * gPixelsPerScanline;
        

        for (i = 0; i < (INT)KeNumberOfProcessors; i++)
        {
            PrintT("Core %i summary: %b\n", i, CoresSchedulerData[i].ThreadReadyQueuesSummary);
        }

        PrintT("\nExits:\n%i\n",exits);

        PrintT("\nI was ran from core %i\n", KeGetCurrentProcessorId());

        KeReleaseSpinLockFromDpcLevel(&pcr->Prcb->Lock);
        KeReleaseSpinLockFromDpcLevel(&ProcessListLock);
        KeLowerIrql(irql);

        TextIoSetColorInformation(oldColor, oldBackground, oldRenderBack);
        TextIoSetCursorPosition(cursorX, cursorY);
        TextIoSetBoundingBox(oldBoundingBox);


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
    test2->Tcb.ThreadPriority = 1;

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

    PrintT("Core %i's idle thread %X\n", coreNumber, *IdleThread);

    (*IdleThread)->Tcb.ThreadPriority = 0;

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
    pPcr->CyclesLeft = (LONG_PTR)KiCyclesPerQuantum * 100;
    KeReleaseSpinLockFromDpcLevel(&pPrcb->Lock);

    PspCoresInitialized++;
    KeReleaseSpinLock(&SchedulerCommonLock, irql);
    
    HalpUpdateThreadKernelStack((PVOID)((ULONG_PTR)pPrcb->IdleThread->KernelStackPointer + sizeof(KTASK_STATE)));
    PagingSetAddressSpace(pPrcb->IdleThread->Process->AddressSpacePhysicalPointer);
    pPrcb->IdleThread->ThreadState = THREAD_STATE_RUNNING;

    if (PspCoresInitialized == KeNumberOfProcessors)
    {
        NTSTATUS status;

        NTSTATUS ObpMpTest();
        status = ObpMpTest();
    }
    
    DisableInterrupts();
    KeLowerIrql(0);
    PspSwitchContextTo64(pPrcb->IdleThread->KernelStackPointer);

    return STATUS_SUCCESS;
}
#pragma warning(pop)

QWORD GetCR8();
VOID SetCR8(QWORD);
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
    
    if (originalRunningThread->ThreadState != THREAD_STATE_RUNNING)
        pcr->CyclesLeft = 0;

    originalRunningProcess = originalRunningThread->Process;
    if (originalRunningProcess == NULL)
    {
        KeBugCheckEx(WORKER_THREAD_TEST_CONDITION, (ULONG_PTR)originalRunningThread, 0, 0, 0);
    }
    originalRunningProcessSpinlock = &originalRunningProcess->ProcessLock;
    KeAcquireSpinLockAtDpcLevel(originalRunningProcessSpinlock);

    originalRunningThreadPriority = originalRunningThread->ThreadPriority + originalRunningProcess->BasePriority;

    /* if no next thread has been selected or the currently selected thread is not ready */
    if (pcr->Prcb->IdleThread == pcr->Prcb->NextThread || 
        pcr->Prcb->NextThread->ThreadState != THREAD_STATE_READY)
    {

        /* select a new next thread */
        pcr->Prcb->NextThread = PspSelectNextReadyThread(pcr->Prcb->Number);
    }

    if (pcr->CyclesLeft < (LONG_PTR)KiCyclesPerQuantum)
    {
        /* If no next thread was found, or the next thread can't preempt the current one 
         * and the current thread is not waiting, reset the quantum for current thread. */
        if (((pcr->Prcb->NextThread == pcr->Prcb->IdleThread && pcr->Prcb->CurrentThread != pcr->Prcb->IdleThread) ||
            (originalRunningThreadPriority > pcr->Prcb->NextThread->ThreadPriority + pcr->Prcb->NextThread->Process->BasePriority)) &&
            pcr->Prcb->CurrentThread->ThreadState == THREAD_STATE_RUNNING)
        {
            pcr->CyclesLeft = originalRunningProcess->QuantumReset * KiCyclesPerQuantum;
        }
        else
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

            /* thread could have volountarily given up control, 
             * it could be waiting - then its state shouldn't be changed */
            if (originalRunningThread->ThreadState == THREAD_STATE_RUNNING)
            {
                PspInsertIntoSharedQueue(originalRunningThread);
                originalRunningThread->ThreadState = THREAD_STATE_READY;
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
    KIRQL irql;
    NTSTATUS status;
    PEPROCESS pProcess;
    OBJECT_ATTRIBUTES processObjAttributes;

    /* create process object */
    InitializeObjectAttributes(
        &processObjAttributes,
        NULL,
        0,
        INVALID_HANDLE_VALUE,
        NULL
    );

    status = ObCreateObject(
        &pProcess, 
        0, 
        KernelMode, 
        &processObjAttributes, 
        sizeof(EPROCESS), 
        PsProcessType
    );

    if (status != STATUS_SUCCESS)
        return status;

    /* TODO:
     * Code above should be moved to some NtCreateProcess like function
     * code below should be run as a OnCreate method of the process type object 
     * (automatically by ObCreateObject).
     * Same thing goes for other thread/process creation/deletion functions */

    /* lock the process list */
    KeAcquireSpinLock(&ProcessListLock, &irql);

    /* make sure it is not prematurely used */
    pProcess->Initialized = FALSE;
    KeInitializeSpinLock(&pProcess->Pcb.ProcessLock);
    
    /* lock the process */
    KeAcquireSpinLockAtDpcLevel(&pProcess->Pcb.ProcessLock);

    InitializeDispatcherHeader(&pProcess->Pcb.Header, OBJECT_TYPE_KPROCESS);
    KeInitializeSpinLock(&pProcess->Pcb.ProcessLock);
    InitializeListHead(&pProcess->Pcb.ThreadListHead);
    pProcess->Pcb.BasePriority = 0;
    pProcess->Pcb.AffinityMask = KAFFINITY_ALL;
    pProcess->Pcb.NumberOfThreads = 0;
    pProcess->Pcb.AddressSpacePhysicalPointer = PagingCreateAddressSpace();
    pProcess->Pcb.QuantumReset = 6;
    InitializeListHead(&pProcess->Pcb.HandleDatabaseHead);

    InsertTailList(&ProcessListHead, &pProcess->Pcb.ProcessListEntry);

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
NTSTATUS PspCreateThreadInternal(
    PETHREAD* ppThread, 
    PEPROCESS pParentProcess, 
    BOOL IsKernel, 
    ULONG_PTR EntryPoint
)
{
    KIRQL irql;
    NTSTATUS status;
    PETHREAD pThread;
    ULONG_PTR userstack;
    ULONG_PTR originalAddressSpace;
    OBJECT_ATTRIBUTES threadObjAttributes;

    InitializeObjectAttributes(
        &threadObjAttributes,
        NULL,
        0,
        INVALID_HANDLE_VALUE,
        NULL
    );

    status = ObCreateObject(
        &pThread,
        0,
        KernelMode,
        &threadObjAttributes,
        sizeof(ETHREAD),
        PsThreadType
    );

    if (status != STATUS_SUCCESS)
        return status;

    KeAcquireSpinLock(&ThreadListLock, &irql);

    pThread->Tcb.ThreadState = THREAD_STATE_INITIALIZATION;
    KeInitializeSpinLock(&pThread->Tcb.ThreadLock);
    
    KeAcquireSpinLockAtDpcLevel(&pThread->Tcb.ThreadLock);
    pThread->Process = pParentProcess;
    pThread->StartAddress = 0;

    KeAcquireSpinLockAtDpcLevel(&pThread->Process->Pcb.ProcessLock);

    originalAddressSpace = PagingGetAddressSpace();
    PagingSetAddressSpace(pThread->Process->Pcb.AddressSpacePhysicalPointer);

    /* allocate stack even if in kernel mode */
    if (IsKernel)
        userstack = (ULONG_PTR)PagingAllocatePageFromRange(PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END);
    else
        userstack = (ULONG_PTR)PagingAllocatePageFromRange(PAGING_USER_SPACE, PAGING_USER_SPACE_END);
    
    userstack += (ULONG_PTR)PAGE_SIZE;;

    PagingSetAddressSpace(originalAddressSpace);

    InitializeDispatcherHeader(&pThread->Tcb.Header, OBJECT_TYPE_KTHREAD);

    MemSet(pThread->Tcb.ThreadWaitBlocks, 0, sizeof(pThread->Tcb.ThreadWaitBlocks));
    pThread->Tcb.ThreadPriority = 0;
    pThread->Tcb.NumberOfCurrentWaitBlocks = 0;
    pThread->Tcb.NumberOfActiveWaitBlocks = 0;
    pThread->Tcb.CurrentWaitBlocks = (PKWAIT_BLOCK)NULL;
    pThread->Tcb.WaitStatus = 0;
    pThread->Tcb.Alertable = FALSE;
    pThread->Tcb.Process = &pParentProcess->Pcb;
    pThread->Tcb.Timeout = 0;
    pThread->Tcb.TimeoutIsAbsolute = FALSE;
    pThread->Tcb.KernelStackPointer = (PVOID)(PspCreateKernelStack() - sizeof(KTASK_STATE));
    /* inherit affinity after the parent process */
    pThread->Tcb.Affinity = pThread->Tcb.Process->AffinityMask;
    PspSetupThreadState((PKTASK_STATE)pThread->Tcb.KernelStackPointer, IsKernel, EntryPoint, userstack);

    InsertTailList(&pParentProcess->Pcb.ThreadListHead, &pThread->Tcb.ProcessChildListEntry);
    KeReleaseSpinLockFromDpcLevel(&pThread->Process->Pcb.ProcessLock);

    InsertTailList(&ThreadListHead, &pThread->Tcb.ThreadListEntry);
    
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

    KeAcquireSpinLockAtDpcLevel(&pThread->Tcb.ThreadLock);
    KeAcquireSpinLock(&ThreadListLock, &irql);
    pThread->Tcb.ThreadState = THREAD_STATE_TERMINATED;

    if (LockParent)
        KeAcquireSpinLockAtDpcLevel(&pThread->Process->Pcb.ProcessLock);
    
    RemoveEntryList(&pThread->Tcb.ProcessChildListEntry);
    if (LockParent)
        KeReleaseSpinLockFromDpcLevel(&pThread->Process->Pcb.ProcessLock);

    RemoveEntryList(&pThread->Tcb.ThreadListEntry);

    KeReleaseSpinLockFromDpcLevel(&pThread->Tcb.ThreadLock);
    KeReleaseSpinLock(&ThreadListLock, irql);
    ObDereferenceObject(pThread);

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
    PLIST_ENTRY current;
    PHANDLE_DATABASE currentHandleDatabase;

    KeAcquireSpinLock(&ProcessListLock, &irql);

    KeAcquireSpinLockAtDpcLevel(&pProcess->Pcb.ProcessLock);
    
    current = pProcess->Pcb.ThreadListHead.First;
    while (current != &pProcess->Pcb.ThreadListHead)
    {
        PETHREAD Thread;
        Thread = (PETHREAD)((ULONG_PTR)current - FIELD_OFFSET(KTHREAD, ProcessChildListEntry));
        current = current->Next;
        
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
            ObCloseHandleByEntry(&currentHandleDatabase->Entries[i]);
        }

        ExFreePool(currentHandleDatabase);
        currentHandleDatabase = next;
    }

    RemoveEntryList(&pProcess->Pcb.ProcessListEntry);

    InternalFreePhysicalPage(pProcess->Pcb.AddressSpacePhysicalPointer);

    KeReleaseSpinLockFromDpcLevel(&pProcess->Pcb.ProcessLock);
    KeReleaseSpinLock(&ProcessListLock, irql);

    ObDereferenceObject(pProcess);
    return STATUS_SUCCESS;
}

VOID PspInsertIntoSharedQueue(PKTHREAD Thread)
{
    UCHAR ThreadPriority;
    KIRQL irql;

    KeAcquireSpinLock(&PspSharedReadyQueue.Lock, &irql);

    ThreadPriority = (UCHAR)(Thread->ThreadPriority + (CHAR)Thread->Process->BasePriority);
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

    if (Thread->NumberOfActiveWaitBlocks == 0 && Thread->ThreadState == THREAD_STATE_WAITING)
    {
        Thread->Timeout = 0;
        Thread->TimeoutIsAbsolute = 0;
        if (Thread->NumberOfCurrentWaitBlocks)
        {
            Thread->NumberOfCurrentWaitBlocks = 0;
            Thread->CurrentWaitBlocks = 0;
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
                PLIST_ENTRY current = sharedReadyQueues[checkedPriority].EntryListHead.First;
                
                while (current != &sharedReadyQueues[checkedPriority].EntryListHead)
                {
                    thread = (PKTHREAD)((ULONG_PTR)current - FIELD_OFFSET(KTHREAD, ReadyQueueEntry));
                    
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

                    current = current->Next;
                }
            }

            if (result)
                break;
        }
    }

    KeReleaseSpinLock(&PspSharedReadyQueue.Lock, irql);

    return result;
}

VOID PsExitThread(DWORD exitCode)
{
    PKTHREAD currentThread;
    KIRQL originalIrql;
    PKWAIT_BLOCK waitBlock;
    PLIST_ENTRY waitEntry;
    PKTHREAD waitingThread;

    /* raise irql as the CPU shouldn't try to schedule here */
    KeRaiseIrql(DISPATCH_LEVEL, &originalIrql);
    
    currentThread = KeGetCurrentThread();
    KeAcquireSpinLockAtDpcLevel(&ProcessListLock);
    exits++;
    KeAcquireSpinLockAtDpcLevel(&currentThread->ThreadLock);
    KeAcquireSpinLockAtDpcLevel(&currentThread->Header.Lock);
    currentThread->ThreadState = THREAD_STATE_TERMINATED;
    currentThread->ThreadExitCode = exitCode;
    
    while (!IsListEmpty(&currentThread->Header.WaitHead))
    { 
        waitEntry = (PLIST_ENTRY)currentThread->Header.WaitHead.First;
        waitBlock = (PKWAIT_BLOCK)waitEntry;
        waitingThread = waitBlock->Thread;
        
        KeAcquireSpinLockAtDpcLevel(&waitingThread->ThreadLock);
        waitingThread->NumberOfActiveWaitBlocks--;
        PrintT("Newstate: %i by core %i\n", waitingThread->NumberOfActiveWaitBlocks, KeGetCurrentProcessorId());
        KeReleaseSpinLockFromDpcLevel(&waitingThread->ThreadLock);

        PsCheckThreadIsReady(waitingThread);
        RemoveEntryList(waitEntry);
        MemSet(waitBlock, 0, sizeof(*waitBlock));
    }

    currentThread->Header.SignalState = INT32_MAX;
    KeReleaseSpinLockFromDpcLevel(&currentThread->Header.Lock);
    KeReleaseSpinLockFromDpcLevel(&currentThread->ThreadLock);
    KeReleaseSpinLockFromDpcLevel(&ProcessListLock);
    KeLowerIrql(originalIrql);
    PspSchedulerNext();
}
