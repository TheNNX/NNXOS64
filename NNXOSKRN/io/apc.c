#include <apc.h>
#include <scheduler.h>
#include <cpu.h>
#include <bugcheck.h>


__declspec(noreturn)
VOID
KiUserApcDispatcher(
    NORMAL_ROUTINE NormalRoutine,
    PVOID NormalContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2,
    PKTASK_STATE pTaskState
);

VOID
NTAPI
KeInitializeApcState(
    PKAPC_STATE pApcState)
{
    INT i;

    for (i = 0; i < 2; i++)
        InitializeListHead(&pApcState->ApcListHeads[i]);

    pApcState->KernelApcsDisabled = FALSE;
    pApcState->UserApcsDisabled = FALSE;
    pApcState->KernelApcInProgress = FALSE;
    pApcState->KernelApcPending = FALSE;
    pApcState->UserApcPending = FALSE;
}


BOOLEAN
NTAPI
KiInsertQueueAPC(
    PKAPC Apc,
    LONG Increment)
{
    PKTHREAD pThread;
    BOOL success;

    if (!LOCKED(DispatcherLock))
    {
        KeBugCheckEx(
            SPIN_LOCK_NOT_OWNED,
            __LINE__,
            (ULONG_PTR)&DispatcherLock,
            0,
            0);
    }

    pThread = Apc->Thread;
    KeAcquireSpinLockAtDpcLevel(&pThread->ThreadLock);

    success = FALSE;

    if (Apc->Inserted == FALSE)
    {
        /* special kernel APC */
        if (Apc->NormalRoutine == NULL && 
            Apc->ApcMode == KernelMode)
        {
            InsertTailList(
                &pThread->ApcStatePointers[Apc->ApcStateIndex]->ApcListHeads[Apc->ApcMode],
                &Apc->ApcListEntry
            );
        }

        /* regular kernel APC and user APC */
        else
        {
            InsertTailList(
                &pThread->ApcStatePointers[Apc->ApcStateIndex]->ApcListHeads[Apc->ApcMode],
                &Apc->ApcListEntry
            );
        }

        if (Apc->ApcStateIndex == pThread->ApcStateIndex &&
            Apc->ApcMode == KernelMode)
        {
            pThread->ApcStatePointers[Apc->ApcStateIndex]->KernelApcPending = TRUE;
        }

        if (pThread->ThreadState == THREAD_STATE_WAITING)
        {
            if (pThread->Alertable == TRUE || Apc->ApcMode == KernelMode)
            {
                if (Apc->ApcMode == UserMode)
                {
                    pThread->ApcStatePointers[Apc->ApcStateIndex]->UserApcPending = TRUE;
                    KeUnwaitThreadNoLock(
                        pThread,
                        STATUS_USER_APC,
                        Increment
                    );
                }
                else
                {
                    KeUnwaitThreadNoLock(
                        pThread,
                        STATUS_KERNEL_APC,
                        Increment
                    );
                }
            }
        }

        Apc->Inserted = TRUE;
        success = TRUE;
    }

    if (success && 
        pThread->ApcState.KernelApcPending &&
        pThread->ThreadState == THREAD_STATE_RUNNING)
    {
        /* TODO: send IPI to the core executing the thread to notify about the 
         * pending kernel APC. */
    }

    KeReleaseSpinLockFromDpcLevel(&pThread->ThreadLock);
    return success;
}

VOID
KiCopyContextToUserStack(
    PKTHREAD pThread)
{
    PKTASK_STATE destination;
    PKTASK_STATE source;
    INT i;

    /* check if the thread is locked */
    if (!LOCKED(pThread->ThreadLock))
        KeBugCheckEx(SPIN_LOCK_NOT_OWNED, __LINE__, 0, 0, 0);

    source = pThread->KernelStackPointer;
    /* get the user stack */
    destination = (PKTASK_STATE)source->Rsp;

    for (i = 0; i < sizeof(PKTASK_STATE) / sizeof(ULONG_PTR); i++)
    {
        KiSetUserMemory(((ULONG_PTR*)destination) + i, ((ULONG_PTR*)source)[i]);
    }

    /* modify the user stack */
    source->Rsp -= sizeof(KTASK_STATE);
}

VOID
NTAPI
KiExecuteUserApcNormalRoutine(
    PKTHREAD pThread,
    NORMAL_ROUTINE NormalRoutine,
    PVOID NormalContext,
    PVOID SystemArguemnt1,
    PVOID SystemArgument2)
{
    PKTASK_STATE currentThreadTaskState;
    ULONG_PTR usercallParameters[5];

    /* TODO: check the order of bugcheck code arguments */
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
        KeBugCheckEx(
            IRQL_NOT_GREATER_OR_EQUAL,
            DISPATCH_LEVEL, 
            KeGetCurrentIrql(), 
            0, 
            0);

    /* this is in, a way, the fifth parameter */
    KiCopyContextToUserStack(pThread);
    
    currentThreadTaskState = (PKTASK_STATE)pThread->KernelStackPointer;

    /* set the parameters for the user stub */
    usercallParameters[0] = (ULONG_PTR)NormalRoutine;
    usercallParameters[1] = (ULONG_PTR)NormalContext;
    usercallParameters[2] = (ULONG_PTR)SystemArguemnt1;
    usercallParameters[3] = (ULONG_PTR)SystemArgument2;
    usercallParameters[4] = (ULONG_PTR)currentThreadTaskState->Rsp;
    
    PspUsercall(
        pThread, 
        KiUserApcDispatcher,
        usercallParameters, 
        5, 
        NULL
    );
}

VOID
NTAPI
KeDeliverApcs(
    KPROCESSOR_MODE PreviousMode)
{
    PKTHREAD currentThread;
    KIRQL irql;
    NORMAL_ROUTINE normalRoutine;
    KERNEL_ROUTINE KernelRoutine;
    PVOID sysArg1, sysArg2;
    PVOID normalContext;

    currentThread = KeGetCurrentThread();

    /* lock the thread (the caller should also reference it) */
    KeAcquireSpinLock(&currentThread->ThreadLock, &irql);

    if (!IsListEmpty(&currentThread->ApcState.ApcListHeads[UserMode]))
    {
        if (currentThread->ApcState.UserApcPending == TRUE && PreviousMode == UserMode)
        {
            PKAPC apc;

            apc = (PKAPC)currentThread->ApcState.ApcListHeads[UserMode].First;
            KernelRoutine = apc->KernelRoutine;
            normalContext = apc->NormalContext;
            normalRoutine = apc->NormalRoutine;
            sysArg1 = apc->SystemArgument1;
            sysArg2 = apc->SystemArgument2;

            /* release the lock to lower the IRQL to APC_LEVEL */
            KeReleaseSpinLock(&currentThread->ThreadLock, irql);
            KernelRoutine(
                apc,
                &normalRoutine, 
                &normalContext, 
                &sysArg1, 
                &sysArg2
            );
            /* acquire the lock back (the thread should be referenced) */
            KeAcquireSpinLock(&currentThread->ThreadLock, &irql);

            currentThread->ApcState.UserApcPending = FALSE;
            RemoveEntryList(&apc->ApcListEntry);
            apc->Inserted = FALSE;

            KiExecuteUserApcNormalRoutine(
                currentThread,
                normalRoutine, 
                normalContext, 
                sysArg1, 
                sysArg2
            );
        }
    }

    KeReleaseSpinLock(&currentThread->ThreadLock, irql);
}