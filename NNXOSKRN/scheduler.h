#ifndef NNX_SHEDULER_HEADER
#define NNX_SHEDULER_HEADER

#include <HAL/cpu.h>
#include <HAL/spinlock.h>
#include <ob/object.h>
#include <ob/handle.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THREAD_WAIT_OBJECTS 3

#define THREAD_STATE_INITIALIZATION 0
#define THREAD_STATE_READY			1
#define THREAD_STATE_RUNNING		2
#define THREAD_STATE_TERMINATED		4
#define THREAD_STATE_WAITING		5

	typedef struct _KPROCESS
	{
		DISPATCHER_HEADER Header;
		KSPIN_LOCK ProcessLock;
		/**
		 * @brief Base process priority used to compute thread priority.
		*/
		UCHAR BasePriority;
		KAFFINITY AffinityMask;
		ULONG_PTR AddressSpacePhysicalPointer;
		LIST_ENTRY ThreadListHead;
		UINT64 NumberOfThreads;
		/**
		 * @brief Stores quantum units (3rds of timer interval), multiply by KiCyclesPerQuantum to get value for CyclesLeft.
		*/
		ULONG_PTR QuantumReset;
		NTSTATUS ProcessResult;
        LIST_ENTRY HandleDatabaseHead;
    } KPROCESS, *PKPROCESS;

	typedef struct _EPROCESS 
	{
		KPROCESS Pcb;
		BOOL Initialized;
	} EPROCESS, *PEPROCESS;

	typedef struct _KTHREAD
	{
		DISPATCHER_HEADER Header;
		KSPIN_LOCK ThreadLock;
		PVOID KernelStackPointer;
		PKPROCESS Process;
		CHAR ThreadPriority;
		KWAIT_BLOCK ThreadWaitBlocks[THREAD_WAIT_OBJECTS];
		PKWAIT_BLOCK CustomThreadWaitBlocks;
		ULONG NumberOfCustomThreadWaitBlocks;
		LIST_ENTRY_POINTER WaitHead;
		KAFFINITY Affinity;
		LIST_ENTRY_POINTER ReadyQueueEntry;
		
		/**
		 * @brief in hundreds of nanoseconds;
		 * if absolute, since 1st of January, 1601 
		*/
		ULONG64 Timeout;
		BOOL TimeoutIsAbsolute;

		UCHAR ThreadState;
		DWORD ThreadExitCode;

		/* TODO */
		BOOL Alertable;
		NTSTATUS WaitStatus;
	} KTHREAD, *PKTHREAD;

	typedef struct _ETHREAD
	{
		KTHREAD Tcb;
		PEPROCESS Process;
		PVOID StartAddress;
	} ETHREAD, *PETHREAD;

	/**
	 * @brief Architecture dependent 
	*/
	typedef struct _KTASK_STATE
	{
#ifdef _M_AMD64
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
		UINT64 Rsp;
		UINT64 Ss;
#else
#error "Architecture unsupported"
#endif
	}KTASK_STATE, *PKTASK_STATE;

	__declspec(noreturn) VOID PspSwitchContextTo64(PVOID StackWithContext);
	NTSTATUS PspDebugTest();

	PKTHREAD PspGetCurrentThread();
	PKTHREAD KeGetCurrentThread();
	
	/**
	 * @brief Voluntarily ends the calling thread's quantum (that doesn't mean it can't get another one, caller has to ensure it is in wait state)
	*/
	VOID PspSchedulerNext();
	
	NTSTATUS PspInitializeCore(UINT8 CoreNumber);

	/**
	 * @brief First it checks if there are threads pending in the shared 
	 * If thread is no longer waiting for anything, clears all wait-related members inside the thread structure (except the WaitStatus) and marks it as ready.
	 * Then it tries to insert it into the shared queue.
	 * @param Thread 
	 * @return TRUE if Thread became ready after calling, FALSE otherwise
	*/
	BOOL PsCheckThreadIsReady(PKTHREAD Thread);

	/**
	 * @brief Checks shared queue for any threads with higher priority than those in processor's queue
	 * @param CoreNumber - Core with the destination queue
	 * @return TRUE if a potential thread in the shared queue was found.
	*/
	BOOL PspManageSharedReadyQueue(UCHAR CoreNumber);
#ifdef __cplusplus
}
#endif

#endif