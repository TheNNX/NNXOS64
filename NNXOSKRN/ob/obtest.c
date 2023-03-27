#include <scheduler.h>
#include <pcr.h>
#include <bugcheck.h>

NTSTATUS PspCreateProcessInternal(PEPROCESS* ppProcess);
NTSTATUS PspCreateThreadInternal(
    PETHREAD* ppThread,
    PEPROCESS pParentProcess,
    BOOL IsKernel,
    ULONG_PTR EntryPoint
);

#define WORKER_PROCESSES_NUMBER 15
#define WORKER_THREADS_PER_PROCESS 15

UINT NextTesterId = 0;
KSPIN_LOCK NextTesterIdLock;

NTSTATUS WorkerStatus[WORKER_THREADS_PER_PROCESS * WORKER_PROCESSES_NUMBER];
PETHREAD WorkerThreads[WORKER_THREADS_PER_PROCESS * WORKER_PROCESSES_NUMBER];
KWAIT_BLOCK WaitBlocks[WORKER_PROCESSES_NUMBER * WORKER_THREADS_PER_PROCESS];
PEPROCESS WorkerProcesses[WORKER_PROCESSES_NUMBER];


static UNICODE_STRING ObpTestPath = RTL_CONSTANT_STRING(L"ObjectTypes\\Directory");
static UNICODE_STRING ObpInvalidTestPath = RTL_CONSTANT_STRING(L"ObjectTypes\\");
static UNICODE_STRING ObpNonExistentTestPath = RTL_CONSTANT_STRING(L"ObjectTypes\\Nonexistent");

NTSTATUS ObpTestNamespace()
{
    NTSTATUS status;
    PVOID globalNamespaceObject;
    PVOID directoryTypeObject;
    PVOID invalidObject;

    /* get the pointer to global namespace */
    status = ObReferenceObjectByHandle(
        GlobalNamespace,
        0,
        ObDirectoryObjectType,
        KernelMode,
        &globalNamespaceObject,
        NULL
    );

    if (status != STATUS_SUCCESS)
        return status;

    /* try opening one of the predefined objects */
    status = ObDirectoryObjectType->ObjectOpen(
        globalNamespaceObject,
        &directoryTypeObject,
        0,
        KernelMode,
        &ObpTestPath,
        TRUE
    );

    if (status != STATUS_SUCCESS)
        return status;


    /* try opening an invalid object */
    status = ObDirectoryObjectType->ObjectOpen(
        globalNamespaceObject,
        &invalidObject,
        0,
        KernelMode,
        &ObpInvalidTestPath,
        TRUE
    );

    if (status != STATUS_OBJECT_PATH_INVALID)
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, __LINE__, status, STATUS_OBJECT_PATH_INVALID, 0);


    /* try opening a non-existent object */
    status = ObDirectoryObjectType->ObjectOpen(
        globalNamespaceObject,
        &invalidObject,
        0,
        KernelMode,
        &ObpNonExistentTestPath,
        TRUE
    );

    if (status != STATUS_OBJECT_NAME_NOT_FOUND)
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, __LINE__, status, STATUS_OBJECT_NAME_NOT_FOUND, 0);

    /* this too, theoreticaly, can fail (on deletion, but we don't expect deletion anyway) */
    status = ObDereferenceObject(globalNamespaceObject);
    if (status != STATUS_SUCCESS)
        return status;

    return STATUS_SUCCESS;
}

static 
NTSTATUS 
Test1(UINT ownId)
{
    INT i;
    NTSTATUS status;

    for (i = 0; i < 64; i++)
    {
        status = ObpTestNamespace();
        if (status != STATUS_SUCCESS)
        {
            return status;
        }
    }

    return STATUS_SUCCESS;
}

static 
NTSTATUS 
Test2()
{
    INT i;
    NTSTATUS status;

    for (i = 0; i < 64; i++)
    {
        PVOID object;
        OBJECT_ATTRIBUTES objAttribs;
        InitializeObjectAttributes(&objAttribs, NULL, OBJ_KERNEL_HANDLE, INVALID_HANDLE_VALUE, 0);

        status = ObCreateObject(
            &object,
            0,
            UserMode,
            &objAttribs,
            ObTypeObjectType,
            NULL
        );

        if (status != STATUS_SUCCESS)
            return status;

        status = ObDereferenceObject(object);
        if (status != STATUS_SUCCESS)
            return status;
    }

    return STATUS_SUCCESS;
}

static VOID ObpWorkerThreadFunction()
{
    UINT ownId;
    KIRQL irql;
    NTSTATUS status;

    /* acquire lock for getting the worker id */
    KeAcquireSpinLock(&NextTesterIdLock, &irql);
    PrintT("[%i] started ", NextTesterId);
    ownId = NextTesterId++;
    KeReleaseSpinLock(&NextTesterIdLock, irql);

    status = Test1(ownId);
    if (status)
        PsExitThread((DWORD)status);

    status = Test2();
    if (status)
        PsExitThread((DWORD)status);

    PsExitThread(0);
}

/* @brief this function should be run in a thread */
NTSTATUS ObpMpTestNamespaceThread()
{
    INT i, j;
    NTSTATUS waitStatus;
    NTSTATUS creationStatus;
    KIRQL irql;

    KeInitializeSpinLock(&NextTesterIdLock);

    /* initialize worker threads with some invalid status values */
    for (i = 0; i < WORKER_THREADS_PER_PROCESS * WORKER_PROCESSES_NUMBER; i++)
        WorkerStatus[i] = (NTSTATUS)(-1);

    for (i = 0; i < WORKER_PROCESSES_NUMBER; i++)
    {
        KeAcquireSpinLock(&NextTesterIdLock, &irql);

        PrintT("Creating process %i from CPU%i\n", i, KeGetCurrentProcessorId());

        creationStatus = PspCreateProcessInternal(&WorkerProcesses[i]);

        if (!NT_SUCCESS(creationStatus))
        {
            PrintT("[%s:%i] creation failed for process %i\n", __FILE__, __LINE__, i);
            KeReleaseSpinLock(&NextTesterIdLock, irql);
            return creationStatus;
        }

        for (j = 0; j < WORKER_THREADS_PER_PROCESS; j++)
        {
            creationStatus = PspCreateThreadInternal(
                WorkerThreads + j + i * WORKER_THREADS_PER_PROCESS,
                WorkerProcesses[i],
                TRUE,
                (ULONG_PTR)ObpWorkerThreadFunction
            );

            if (!NT_SUCCESS(creationStatus))
            {
                PrintT("[%s:%i] creation failed for thread %i:%i\n", __FILE__, __LINE__, i, j);
                KeReleaseSpinLock(&NextTesterIdLock, irql);
                return creationStatus;
            }

            WorkerThreads[j + i * WORKER_THREADS_PER_PROCESS]->Tcb.ThreadPriority = 5;
            ObReferenceObject(WorkerThreads[j + i * WORKER_THREADS_PER_PROCESS]);

            PspInsertIntoSharedQueueLocked(&WorkerThreads[j + i * WORKER_THREADS_PER_PROCESS]->Tcb);
        }

        KeReleaseSpinLock(&NextTesterIdLock, irql);
    }

    /* wait for all the threads */
    waitStatus = KeWaitForMultipleObjects(
        WORKER_PROCESSES_NUMBER * WORKER_THREADS_PER_PROCESS,
        WorkerThreads,
        WaitAll,
        Executive,
        KernelMode,
        FALSE,
        NULL,
        WaitBlocks
    );


    if (waitStatus != STATUS_SUCCESS)
        PsExitThread(waitStatus);


    for (i = 0; i < WORKER_PROCESSES_NUMBER * WORKER_THREADS_PER_PROCESS; i++)
    {
        if (WorkerThreads[i]->Tcb.ThreadExitCode != STATUS_SUCCESS)
            PsExitThread(WorkerThreads[i]->Tcb.ThreadExitCode);

        ObDereferenceObject(WorkerThreads[i]);
    }

    for (i = 0; i < WORKER_PROCESSES_NUMBER; i++)
    {
        ObDereferenceObject(WorkerProcesses[i]);
    }

    PrintT("Success\n");
    /* deallocation on return from those all things */
    PsExitThread(0);
    return STATUS_SUCCESS;
}

NTSTATUS ObpMpTestNamespace()
{
    PEPROCESS process;
    PETHREAD testThread;
    NTSTATUS status;

    if (KeGetCurrentProcessorId() != 0)
        return STATUS_SUCCESS;

    PrintT("%s %i %i\n", __FUNCTION__, KeGetCurrentProcessorId(), KeGetCurrentIrql());

    status = PspCreateProcessInternal(&process);
    if (status != STATUS_SUCCESS)
    {
        PrintT("NTSTATUS: %X\n", status);
        return status;
    }
    PrintT("Created process\n");
    status = PspCreateThreadInternal(&testThread, process, TRUE, (ULONG_PTR)ObpMpTestNamespaceThread);
    if (status != STATUS_SUCCESS)
    {
        PrintT("NTSTATUS: %X\n", status);
        return status;
    }
    PrintT("Created namespace test thread %X\n", testThread);

    testThread->Process->Pcb.BasePriority = 10;
    testThread->Tcb.ThreadPriority = 6;
    PspInsertIntoSharedQueueLocked(&testThread->Tcb);

    return STATUS_SUCCESS;
}