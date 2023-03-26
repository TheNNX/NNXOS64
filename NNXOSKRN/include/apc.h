#ifndef NNX_APC_HEADER
#define NNX_APC_HEADER

#include <nnxtype.h>
#include <ntlist.h>
#include <cpu.h>
#include <scheduler.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef VOID (*NORMAL_ROUTINE)(PVOID NormalContext, PVOID SysArg1, PVOID SysArg2);
    typedef NORMAL_ROUTINE* PNORMAL_ROUTINE;
    typedef VOID (*KERNEL_ROUTINE)(struct _KAPC* pApc, PNORMAL_ROUTINE pNormalRoutine, PVOID* pNormalContext, PVOID* pSysArg1, PVOID* pSysArg2);
    typedef KERNEL_ROUTINE* PKERNEL_ROUTINE;
    typedef VOID (*RUNDOWN_ROUTINE)(struct _KAPC* pApc);
    typedef RUNDOWN_ROUTINE* PRUNDOWN_ROUTINE;

    typedef struct _KAPC
    {
        UCHAR Type;
        UCHAR SpareByte0;
        UCHAR Size;
        UCHAR SpareByte1;
        ULONG SpareLong0;
        struct _KTHREAD* Thread;
        LIST_ENTRY ApcListEntry;
        KERNEL_ROUTINE KernelRoutine;
        RUNDOWN_ROUTINE RundownRoutine;
        NORMAL_ROUTINE NormalRoutine;
        PVOID NormalContext;
        PVOID SystemArgument1;
        PVOID SystemArgument2;
        CCHAR ApcStateIndex;
        KPROCESSOR_MODE ApcMode;
        BOOLEAN Inserted;
    } KAPC, *PKAPC;

    typedef enum _KAPC_ENVIRONMENT
    {
        OriginalApcEnvironment,
        AttachedApcEnvironment,
        CurrentApcEnvironment
    } KAPC_ENVIRONMENT, *PKAPC_ENVIRONMENT;

#ifdef NNX_KERNEL

    VOID
    NTAPI
    KeInitializeApcState(PKAPC_STATE pApcState);

    BOOLEAN
    NTAPI
    KiInsertQueueAPC(
        PKAPC Apc,
        LONG Increment);

    VOID
    NTAPI
    KiExecuteUserApcNormalRoutine(
        PKTHREAD pThread,
        NORMAL_ROUTINE NormalRoutine,
        PVOID NormalContext,
        PVOID SystemArguemnt1,
        PVOID SystemArgument2);

    VOID
    NTAPI
    KeDeliverApcs(
        KPROCESSOR_MODE PreviousMode);
#endif

#ifdef __cplusplus
}
#endif
#endif