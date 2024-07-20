#include <nnxtype.h>
#include <dispatcher.h>
#include <ntqueue.h>
#include <ntdebug.h>
#include <syscall.h>

#include <syscall.h>
#include <spinlock.h>
#include <HALX64/include/msr.h>
#include <scheduler.h>
#include <dispatcher.h>
#include <ntqueue.h>
#include <ntdebug.h>
#include <SimpleTextIO.h>
#include <pcr.h>

static KSPIN_LOCK SystemCallSpinLock;
SYSCALL_HANDLER HalpSystemCallHandler;
VOID HalpSystemCall();

volatile long QueueInitialized = FALSE;
static KQUEUE Queue;
static KSPIN_LOCK SystemCallSpinLock = { 0 };


/**
 * @brief This is an architecture dependent implementation of the function
 * for system call initialization. It sets up all the neccessary registers
 * for the system call to be callable.
 */
VOID
NTAPI
HalInitializeSystemCallForCurrentCore(ULONG_PTR SyscallStub)
{
#ifdef _M_AMD64
    KIRQL irql;

    /* Enable syscalls */
    __writemsr(IA32_EFER, __readmsr(IA32_EFER) | 1);

    /* Disable interrupts in the syscall dispatcher routine to avoid
     * race conditions when switching the GSBase and KernelGSBase MSRs */
    __writemsr(IA32_FMASK, (UINT64)(0x200ULL));

    /* Set CS/SS and syscall selectors */
    __writemsr(IA32_STAR, (0x13ULL << 48) | (0x08ULL << 32));

    /* Set the syscall dispatcher routine */
    __writemsr(IA32_LSTAR, SyscallStub);

    KeAcquireSpinLock(&SystemCallSpinLock, &irql);
    if (QueueInitialized == FALSE)
    {
        QueueInitialized = TRUE;
        KeInitializeQueue(&Queue, 0);
    }
    KeReleaseSpinLock(&SystemCallSpinLock, irql);
#endif
}

/**
 * @brief Sets the system call handler.
 * @return Pointer to the previous system call handler.
 */
SYSCALL_HANDLER
NTAPI
SetupSystemCallHandler(
    SYSCALL_HANDLER SystemRoutine)
{
    SYSCALL_HANDLER oldHandler = HalpSystemCallHandler;
    HalpSystemCallHandler = SystemRoutine;
    return oldHandler;
}

NTSTATUS NtDll5Test(
    ULONG_PTR Arg1,
    ULONG_PTR Arg2,
    ULONG_PTR Arg3,
    ULONG_PTR Arg4,
    ULONG_PTR Arg5)
{
    PrintT(__FUNCTION__":%i: %X %X %X %X %X\n", Arg1, Arg2, Arg3, Arg4, Arg5);

    return STATUS_NOT_SUPPORTED;
}

NTSTATUS
NTAPI
KiInvokeServiceHelper(
    ULONG_PTR Arg0,
    ULONG_PTR Arg1,
    ULONG_PTR Service,
    ULONG_PTR NumberOfStackArgs,
    ULONG_PTR AdjustedUserStack);

NTSTATUS KiInvokeService(
    NTSTATUS(NTAPI *Service)(),
    SIZE_T NumberOfArgs, 
    ULONG_PTR Rdx,
    ULONG_PTR R8,
    ULONG_PTR Stack)
{
    switch (NumberOfArgs)
    {
    case 0:
        return Service();
    case 1:
        return Service(Rdx);
    case 2:
        return Service(Rdx, R8);
    case 3:
        return KiInvokeServiceHelper(
            Rdx,
            R8,
            (ULONG_PTR)Service,
            NumberOfArgs - 2,
            /* + ReturnAddress + ShadowZone */
            Stack + 8 + 32);
    default:
        ASSERT(FALSE);
        return 0;
    }
}

/**
 * @brief The default system call handler - note: it is possible
 * to use different handlers, and even to chain load them - to
 * do so, set a different handler with SetupSystemCallHandler,
 * preserve the old handler, and call the old handler from within
 * the new handler.
 */
ULONG_PTR
NTAPI
SystemCallHandler(
    ULONG_PTR R9,
    ULONG_PTR Rdx,
    ULONG_PTR R8,
    ULONG_PTR Rsp)
{
    ULONG_PTR result = 0;
    LONG64 timeout = -10000000;
    ULONG threadId = (ULONG_PTR)KeGetCurrentThread() & 0xFFFF;
    NTSTATUS status;

    if (!_InterlockedCompareExchange(&QueueInitialized, TRUE, FALSE))
    {
        KeInitializeQueue(&Queue, 0);
    }

    VOID KiPrintSpinlockDebug();

    switch (R9)
    {
    case 1:
    case 2:
        PrintT(
            "s%X,%i(%i) ",
            threadId, 
            KeGetCurrentProcessorId(), 
            KeGetCurrentIrql());
        status = KeWaitForSingleObject(
            &Queue, 
            Executive,
            KernelMode,
            FALSE, 
            &timeout);

        ASSERT(status == STATUS_TIMEOUT);

        break;
    case 3:
        return KiInvokeService(NtDll5Test, 5, Rdx, R8, Rsp);
    case 4:
        PrintT("Ldr Test Syscall called!\n");
        break;
    case 5:
        PrintT("Exiting thread %X\n", KeGetCurrentThread());
        PsExitThread((DWORD)Rdx);
        break;
    default:
        PrintT("Warning: unsupported system call %X(%X,%X,%X)!\n", R9, Rdx, R8, Rsp);
        ASSERT(FALSE);
    }

    return result;
}