#include <nnxtype.h>
#include <dispatcher.h>
#include <ntqueue.h>
#include <ntdebug.h>
#include <syscall.h>
#include <mm.h>
#include <syscall.h>
#include <spinlock.h>
#include <HALX64/include/msr.h>
#include <scheduler.h>
#include <dispatcher.h>
#include <ntqueue.h>
#include <ntdebug.h>
#include <SimpleTextIO.h>
#include <pcr.h>

template<int I>
class SyscallInvoker
{
public:
    template<typename T, typename... Args>
    constexpr static inline auto Invoke(PULONG_PTR Rsp, T f, Args... rest)
    {
        return SyscallInvoker<I - 1>::Invoke(Rsp + 1, f, rest..., Rsp[0]);
    }
};

typedef ULONG_PTR(NTAPI *DUMB_TYPE)(...);

template<>
class SyscallInvoker<0> 
{
public:
    template<typename T, typename... Args>
    constexpr static inline auto Invoke(PULONG_PTR Rsp, T f, Args... rest)
    {
        return ((DUMB_TYPE)f)(rest...);
    }
};

template<int argsCount>
ULONG_PTR
Invoke(
    ULONG_PTR(NTAPI* Service)(...),
    PULONG_PTR Rsp)
{
    if constexpr (argsCount == 0)
    {
        return Service();
    }
    if constexpr (argsCount == 1)
    {
        return Service(Rsp[1]);
    }
    if constexpr (argsCount == 2)
    {
        return Service(Rsp[1], Rsp[2]);
    }
    if constexpr (argsCount == 3)
    {
        return Service(Rsp[1], Rsp[2], Rsp[3]);
    }
    if constexpr (argsCount == 4)
    {
        return Service(Rsp[1], Rsp[2], Rsp[3], Rsp[4]);
    }
    if constexpr (argsCount == 5)
    {
        return Service(Rsp[1], Rsp[2], Rsp[3], Rsp[4], Rsp[5]);
    }
    if constexpr (argsCount == 6)
    {
        return Service(Rsp[1], Rsp[2], Rsp[3], Rsp[4], Rsp[5], Rsp[6]);
    }
    if constexpr (argsCount == 7)
    {
        return Service(Rsp[1], Rsp[2], Rsp[3], Rsp[4], Rsp[5], Rsp[6], Rsp[7]);
    }
    if constexpr (argsCount == 8)
    {
        return Service(Rsp[1], Rsp[2], Rsp[3], Rsp[4], Rsp[5], Rsp[6], Rsp[7], Rsp[8]);
    }
    return SyscallInvoker<argsCount>::Invoke(Rsp + 1, Service);
}

extern "C" 
{
    SYSCALL_HANDLER HalpSystemCallHandler;
    VOID HalpSystemCall();


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
        /* Enable syscalls */
        __writemsr(IA32_EFER, __readmsr(IA32_EFER) | 1);

        /* Disable interrupts in the syscall dispatcher routine to avoid
         * race conditions when switching the GSBase and KernelGSBase MSRs */
        __writemsr(IA32_FMASK, (UINT64) (0x200ULL));

        /* Set CS/SS and syscall selectors */
        __writemsr(IA32_STAR, (0x13ULL << 48) | (0x08ULL << 32));

        /* Set the syscall dispatcher routine */
        __writemsr(IA32_LSTAR, SyscallStub);
#else
#error UNIMPLEMENTED
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

    NTSTATUS
    NTAPI
    KiInvokeServiceHelper(
        ULONG_PTR Service,
        ULONG_PTR NumberOfStackArgs,
        PULONG_PTR AdjustedUserStack);

    BOOLEAN
    NTAPI
    MmCheckMemoryAccess(
        KPROCESSOR_MODE PrevMode,
        ULONG_PTR AddrStart,
        ULONG_PTR AddrEnd,
        BOOLEAN Write)
    {
        if (AddrEnd < AddrStart)
        {
            return FALSE;
        }

        /* FIXME/TODO */
        return TRUE;
    }

    VOID 
    NTAPI    
    MmLockMemory(ULONG_PTR Start, ULONG_PTR End)
    {
        /* TODO */
    }


    VOID 
    NTAPI    
    MmUnlockMemory(ULONG_PTR Start, ULONG_PTR End)
    {
        /* TODO */
    }
    
    ULONG_PTR 
    NTAPI    
    KeTestSyscall1(
        ULONG_PTR p1,
        ULONG_PTR p2,
        ULONG_PTR p3,
        ULONG_PTR p4,
        ULONG_PTR p5,
        ULONG_PTR p6,
        ULONG_PTR p7,
        ULONG_PTR p8)
    {
        PrintT("P1:%X %X %X %X %X %X %X %X\n", p1, p2, p3, p4, p5, p6, p7, p8);
        return 0x1111111111111111ULL;
    }

    ULONG_PTR 
    NTAPI    
    KeTestSyscall2(
        ULONG_PTR p1,
        ULONG_PTR p2,
        ULONG_PTR p3,
        ULONG_PTR p4,
        ULONG_PTR p5,
        ULONG_PTR p6,
        ULONG_PTR p7,
        ULONG_PTR p8,
        ULONG_PTR p9)
    {
        PrintT("P2:%X %X %X %X %X %X %X %X %X\n", p1, p2, p3, p4, p5, p6, p7, p8, p9);
        return 0x2222222222222222ULL;
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

        switch (R9)
        {
#define SYSCALL(name, number, argsCount) \
        case number:\
            MmLockMemory(Rsp, Rsp + argsCount);\
            MmCheckMemoryAccess(UserMode, Rsp, Rsp + argsCount, TRUE);\
            result = Invoke<argsCount>((DUMB_TYPE)name, (PULONG_PTR)Rsp);\
            MmUnlockMemory(Rsp, Rsp + argsCount);\
            return result;

#define SYSCALL0(name, number) SYSCALL(name, number, 0)
#define SYSCALL1(name, number) SYSCALL(name, number, 1)
#define SYSCALL2(name, number) SYSCALL(name, number, 2)
#define SYSCALL3(name, number) SYSCALL(name, number, 3)

#include <syscall.inc>

        default:
            PrintT("Warning: unsupported system call %X(%X,%X,%X)!\n", R9, Rdx, R8, Rsp);
            ASSERT(FALSE);
        }

        return result;
    }
}