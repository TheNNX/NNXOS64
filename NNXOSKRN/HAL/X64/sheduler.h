#ifndef NNX_SHEDULER_HEADER
#define NNX_SHEDULER_HEADER

#include "cpu.h"
#include "../spinlock.h"
#include "../../klist.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct _KPROCESS
	{
		KSPIN_LOCK ProcessLock;
		UINT64 BasePriority;
		KAFFINITY AffinityMask;
		PVOID AddressSpacePointer;
		KLINKED_LIST ThreadList;
		UINT64 NumberOfThreads;
	} KPROCESS, *PKPROCESS;

	typedef struct _EPROCESS 
	{
		KPROCESS Pcb;
		KLINKED_LIST ThreadList;
	} EPROCESS, *PEPROCESS;

	typedef struct _KTHREAD
	{
		KSPIN_LOCK ThreadLock;
		PVOID KernelStackPointer;
		PKPROCESS Process;
		UINT64 ThreadPriority;
	} KTHREAD, *PKTHREAD;

	typedef struct _ETHREAD
	{
		KTHREAD Tcb;
		PEPROCESS Process;
		PVOID StartAddress;
	} ETHREAD, *PETHREAD;

	typedef struct _KTASK_STATE
	{
		UINT64 Rax;
		UINT64 Rbx;
		UINT64 Rcx;
		UINT64 Rdx;
		UINT64 Rbp;
		UINT64 Rdi;
		UINT64 Rsi;
		UINT64 R8;
		UINT64 R9;
		UINT64 R10;
		UINT64 R11;
		UINT64 R12;
		UINT64 R13;
		UINT64 R14;
		UINT64 R15;
		UINT64 Ds;
		UINT64 Es;
		UINT64 Gs;
		UINT64 Fs;
		UINT64 Rip;
		UINT64 Cs;
		UINT64 Rflags;
	}KTASK_STATE, *PKTASK_STATE;

	typedef struct _KTASK_STATE_USER
	{
		struct _KTASK_STATE;
		UINT64 Rsp;
		UINT64 Ss;
	}KTASK_STATE_USER, *PKTASK_STATE_USER;

	/* Immediately execute the thread */
	__declspec(noreturn) VOID PspExecuteThread(PKTHREAD);
	__declspec(noreturn) VOID PspSwitchContextTo64(PVOID StackWithContext);

	NTSTATUS PspCreateProcessInternal(PEPROCESS* output);
	NTSTATUS PspCreateThreadInternal(PETHREAD* output, PEPROCESS parent);

	__declspec(noreturn) NTSTATUS PspDebugTest();
	__declspec(noreturn) VOID PspTestAsm();

	NTSTATUS PspDestroyProcessInternal(PEPROCESS process);
	NTSTATUS PspDestoryThreadInternal(PETHREAD thread);
#ifdef __cplusplus
}
#endif

#endif