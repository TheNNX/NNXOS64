/* This file defines user mode stubs used by the operating system to interact with usermode applications */

#include <nnxtype.h>
#include <apc.h>
#include <scheduler.h>

#pragma code_seg(push, ".userc")

__declspec(noreturn)
VOID
KiUserRestoreUserContext(
    PKTASK_STATE Context);

__declspec(noreturn)
VOID
KiUserApcDispatcher(
    NORMAL_ROUTINE NormalRoutine, 
    PVOID NormalContext, 
    PVOID SystemArgument1, 
    PVOID SystemArgument2,
    PKTASK_STATE pTaskState)
{
    NormalRoutine(
        NormalContext, 
        SystemArgument1,
        SystemArgument2
    );

    KiUserRestoreUserContext(pTaskState);
}

#pragma code_seg(pop)