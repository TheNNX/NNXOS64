#include "apc.h"
#include <scheduler.h>
#include <HAL/cpu.h>
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
KeInitializeApcState(PKAPC_STATE pApcState)
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


BOOL
KiInsertQueueAPC(
    PKAPC Apc,
    LONG Increment
)
{
    PKTHREAD pThread;
    BOOL success;
    KIRQL irql;

    pThread = Apc->Thread;

    KeAcquireSpinLock(
        &pThread->ThreadLock,
        &irql
    );

    success = FALSE;

    if (Apc->Inserted == FALSE)
    {
        /* TODO 
         *
         * inserting user apc:
         *
         *   if thread is in an alertable wait state:
	     *       exit alertable wait state
	     *       stage apc
         *   if thread is not in an alerable state
	     *       add to queue
         *
         * 
         * entering waits:
         *
         *   if wait is to be alertabe and user apc queue not empty
	     *       do not enter wait
	     *       stage apc
	     *       set wait result as STATUS_USER_APC		
         * 
         * or something like this, reading is hard
         */


        success = TRUE;
    }

    KeReleaseSpinLock(
        &pThread->ThreadLock,
        irql
    );

    return success;
}

VOID
KiCopyContextToUserStack(
    PKTHREAD pThread
)
{
    PKTASK_STATE destination;
    PKTASK_STATE source;
    INT i;

    /* check if the thread is locked */
    if (pThread->ThreadLock == 0)
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
KiExecuteUserApcNormalRoutine(
    PKTHREAD pThread,
    NORMAL_ROUTINE NormalRoutine,
    PVOID NormalContext,
    PVOID SystemArguemnt1,
    PVOID SystemArgument2
)
{
    PKTASK_STATE currentThreadTaskState;
    ULONG_PTR usercallParameters[5];

    /* TODO: check the order of bugcheck code arguments */
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
        KeBugCheckEx(IRQL_NOT_GREATER_OR_EQUAL, DISPATCH_LEVEL, KeGetCurrentIrql(), 0, 0);

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
KeDeliverApcs(
    KPROCESSOR_MODE PreviousMode
)
{
    PKTHREAD currentThread;
    KIRQL irql;
    NORMAL_ROUTINE normalRoutine;
    KERNEL_ROUTINE KernelRoutine;
    PVOID sysArg1, sysArg2;
    PVOID normalContext;

    currentThread = KeGetCurrentThread();

    KeRaiseIrql(DISPATCH_LEVEL, &irql);

    if (!IsListEmpty(&currentThread->ApcState.ApcListHeads[UserMode]))
    {
        if (currentThread->ApcState.UserApcPending == TRUE || PreviousMode != UserMode)
        {
            PKAPC apc;

            apc = (PKAPC)currentThread->ApcState.ApcListHeads[UserMode].First;
            KernelRoutine = apc->KernelRoutine;
            normalContext = apc->NormalContext;
            normalRoutine = apc->NormalRoutine;
            sysArg1 = apc->SystemArgument1;
            sysArg2 = apc->SystemArgument2;

            KernelRoutine(
                apc,
                &normalRoutine, 
                &normalContext, 
                &sysArg1, 
                &sysArg2
            );

            currentThread->ApcState.UserApcPending = FALSE;

            KiExecuteUserApcNormalRoutine(
                currentThread,
                normalRoutine, 
                normalContext, 
                sysArg1, 
                sysArg2
            );
        }
    }

    KeLowerIrql(irql);
}