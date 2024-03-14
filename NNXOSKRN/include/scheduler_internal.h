#include <scheduler.h>

#ifndef NNX_SCHEDULER_INTERNAL_HEADER
#define NNX_SCHEDULER_INTERNAL_HEADER

#ifdef __cplusplus
extern "C"
{
#endif

    NTSTATUS
    NTAPI
    NnxStartUserProcess(
        PCUNICODE_STRING Filepath,
        HANDLE hOutProcess,
        ULONG Priority);

    VOID
    NTAPI
    PspSetupThreadState(
        PKTASK_STATE pThreadState,
        BOOL IsKernel,
        ULONG_PTR EntryPoint,
        ULONG_PTR Userstack);

    NTSTATUS
    NTAPI
    PspCreateProcessInternal(
        PEPROCESS* ppProcess,
        ACCESS_MASK DesiredAccess,
        POBJECT_ATTRIBUTES pObjectAttributes);

    NTSTATUS
    NTAPI
    PspCreateThreadInternal(
        PETHREAD* ppThread,
        PEPROCESS pParentProcess,
        BOOL IsKernel,
        ULONG_PTR EntryPoint);

    NTSTATUS
    NTAPI
    PspProcessOnCreate(
        PVOID SelfObject,
        PVOID CreateData);

    NTSTATUS
    NTAPI
    PspProcessOnCreateNoDispatcher(
        PVOID SelfObject,
        PVOID CreateData);

    NTSTATUS
    NTAPI
    PspProcessOnDelete(
        PVOID SelfObject);

    NTSTATUS
    NTAPI
    PspThreadOnCreate(
        PVOID SelfObject,
        PVOID CreateData);

    NTSTATUS
    NTAPI
    PspThreadOnCreateNoDispatcher(
        PVOID SelfObject,
        PVOID CreateData);

    NTSTATUS
    NTAPI
    PspThreadOnDelete(
        PVOID SelfObject);

    __declspec(noreturn) VOID NTAPI PspTestAsmUser();
    __declspec(noreturn) VOID NTAPI PspTestAsmUserEnd();
    __declspec(noreturn) VOID NTAPI PspTestAsmUser2();
    __declspec(noreturn) VOID NTAPI PspIdleThreadProcedure();

    VOID
    NTAPI
    NnxDummyLdrThread(
        PUNICODE_STRING Filepath);

#ifdef __cplusplus
}
#endif

#endif