#ifndef NNX_SHEDULER_HEADER
#define NNX_SHEDULER_HEADER

#include <HAL/cpu.h>
#include <HAL/spinlock.h>
#include <ob/object.h>
#include <ob/handle.h>
#include <HAL/interrupt.h>
#include <dispatcher/dispatcher.h>

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
        /* Base process priority used to compute thread priority. */
        UCHAR BasePriority;
        KAFFINITY AffinityMask;
        ULONG_PTR AddressSpacePhysicalPointer;
        LIST_ENTRY ThreadListHead;
        UINT64 NumberOfThreads;
        LIST_ENTRY ProcessListEntry;
        /* Stores quantum units (3rds of timer interval),
         * multiply by KiCyclesPerQuantum to get value for CyclesLeft. */
        ULONG_PTR QuantumReset;
        NTSTATUS ProcessResult;
        LIST_ENTRY HandleDatabaseHead;
    } KPROCESS, * PKPROCESS;

    typedef struct _EPROCESS
    {
        KPROCESS Pcb;
        BOOL Initialized;
    } EPROCESS, * PEPROCESS;


    typedef struct _KAPC_STATE
    {
        LIST_ENTRY ApcListHeads[2];
        BOOL KernelApcsDisabled;
        BOOL UserApcsDisabled;
        BOOL UserApcPending;
        BOOL KernelApcPending;
        BOOL KernelApcInProgress;
    } KAPC_STATE, * PKAPC_STATE;


    typedef struct _KTHREAD
    {
        DISPATCHER_HEADER Header;
        KSPIN_LOCK ThreadLock;

        /* Used to store the threads state and to execute interrupts */
        PVOID KernelStackPointer;

        /* Used for deallocating the kernel stack */
        PVOID OriginalKernelStackPointer;
        SIZE_T NumberOfKernelStackPages;

        /* Used to save thread context when executing APCs */
        PVOID ApcBackupKernelStackPointer;

        /* The parent process */
        PKPROCESS Process;

        /* Thread priority added to the parent process' base priority */
        CHAR ThreadPriority;

        /* Threads own wait blocks used if number of wait blocks required
         * is less or equal to THREAD_WAIT_OBJECTS */
        KWAIT_BLOCK ThreadWaitBlocks[THREAD_WAIT_OBJECTS];

        /* Array of currently used wait blocks*/
        PKWAIT_BLOCK CurrentWaitBlocks;
        /* Number of wait blocks in the array */
        ULONG NumberOfCurrentWaitBlocks;
        /* Number of wait blocks without their associated wait satisfied
         * (that is, how many objects are still waited for) */
        ULONG NumberOfActiveWaitBlocks;

        /* Thread affinity */
        KAFFINITY Affinity;
        KAFFINITY UserAffinity;

        /* Various list entries */
        LIST_ENTRY ReadyQueueEntry;
        LIST_ENTRY ProcessChildListEntry;
        LIST_ENTRY ThreadListEntry;

        /* In hundreds of nanoseconds;
         * if absolute, since 1st of January, 1601 */
        ULONG64 Timeout;
        BOOL TimeoutIsAbsolute;

        UCHAR ThreadState;
        DWORD ThreadExitCode;

        KIRQL ThreadIrql;

        BOOL Alertable;
        ULONG64 WaitStatus;

        union
        {
            struct
            {
                KAPC_STATE ApcState;
                KAPC_STATE SavedApcState;
                PKAPC_STATE OriginalApcStatePointer;
                PKAPC_STATE AttachedApcStatePointer;
            };
            struct
            {
                KAPC_STATE ApcStates;
                PKAPC_STATE ApcStatePointers[2];
            };
        };

        CCHAR ApcStateIndex;

    } KTHREAD, * PKTHREAD;

    typedef struct _ETHREAD
    {
        KTHREAD Tcb;
        PEPROCESS Process;
        PVOID StartAddress;
    } ETHREAD, * PETHREAD;

    /**
     * @brief Architecture dependent
    */
    typedef struct _KTASK_STATE
    {
#ifdef _M_AMD64

        UINT64 Ds;
        UINT64 Es;
        UINT64 Gs;
        UINT64 Fs;

        UINT64 Rsi;
        UINT64 Rdi;
        UINT64 Rbp;
        UINT64 R15;
        UINT64 R14;
        UINT64 R13;
        UINT64 R12;
        UINT64 Rbx;

        UINT64 R11;
        UINT64 R9;
        UINT64 R10;
        UINT64 R8;
        UINT64 Rdx;
        UINT64 Rcx;
        UINT64 Rax;

        UINT64 Rip;
        UINT64 Cs;
        UINT64 Rflags;
        UINT64 Rsp;
        UINT64 Ss;
#else
#error "Architecture unsupported"
#endif
    }KTASK_STATE, *PKTASK_STATE;

    NTSTATUS 
    PspDebugTest();

    PKTHREAD 
    PspGetCurrentThread();

    PKTHREAD 
    KeGetCurrentThread();

    __declspec(noreturn) 
    VOID
    PsExitThread(
        DWORD exitCode);

    NTSTATUS 
    PspInitializeCore(
        UINT8 CoreNumber);

    /**
     * @brief First it checks if there are threads pending in the shared
     * If thread is no longer waiting for anything, clears all wait-related 
     * members inside the thread structure (except the WaitStatus) and marks it as ready.
     * Then it tries to insert it into the shared queue.
     * @param Thread
     * @return TRUE if Thread became ready after calling, FALSE otherwise
    */
    BOOL 
    PsCheckThreadIsReady(
        PKTHREAD Thread);

    /**
     * @brief Checks shared queue for any threads with higher priority than those in processor's queue
     * @param CoreNumber - Core with the destination queue
     * @return TRUE if a potential thread in the shared queue was found.
    */
    BOOL 
    PspManageSharedReadyQueue(
        UCHAR CoreNumber);

    /**
     * @brief Inserts the thread into the shared ready queue for its priority.
     * Requires the Dispatcher Lock to be held.
    */
    VOID 
    PspInsertIntoSharedQueue(
        PKTHREAD Thread);

    /**
     * @brief Same as PspInsertIntoSharedQueue, but manages the 
     * Dispatcher Lock by itself.
     */
    VOID
    PspInsertIntoSharedQueueLocked(
        PKTHREAD Thread);

    VOID
    PspSetupThreadState(
        PKTASK_STATE pThreadState,
        BOOL IsKernel,
        ULONG_PTR Entrypoint,
        ULONG_PTR Userstack);

    VOID
    PspUsercall(
        PKTHREAD pThread, 
        PVOID Function, 
        ULONG_PTR* Parameters,
        SIZE_T NumberOfParameters,
        PVOID ReturnAddress);

    BOOL
    KiSetUserMemory(
        PVOID Address,
        ULONG_PTR Data);

    KPROCESSOR_MODE
    PsGetProcessorModeFromTrapFrame(
        PKTASK_STATE TrapFrame);

    VOID
    NTAPI
    KeSetSystemAffinityThread(
        KAFFINITY Affinity);

    VOID
    NTAPI
    KeRevertToUserAffinityThread(VOID);

    ULONG_PTR
    NTAPI
    PspScheduleThread(
        PKINTERRUPT clockInterrupt, 
        PKTASK_STATE stack);

    VOID
    NTAPI
    KeForceClockTick();

    __declspec(noreturn)
    VOID
    NTAPI
    HalpApplyTaskState(
        PKTASK_STATE TaskState);


#ifdef __cplusplus
}
#endif

#endif