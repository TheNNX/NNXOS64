#include <HAL/syscall.h>
#include <HAL/spinlock.h>
#include "cpu.h"
#include "GDT.h"

static KSPIN_LOCK SystemCallSpinLock;

VOID (*HalpSystemCallHandler)();
VOID HalpSystemCall();

/**
 * @brief This is an architecture dependent implementation of the function
 * for system call initialization. It sets up all the neccessary registers
 * for the system call to be callable. 
 */
VOID HalInitializeSystemCallForCurrentCore()
{
    /* enable syscall */
    HalX64WriteMsr(IA32_EFER, HalX64ReadMsr(IA32_EFER) | 1);

    /* disable interrupts in the syscall dispatcher routine to avoid
     * race conditions when switching the GSBase and KernelGSBase MSRs */
    HalX64WriteMsr(IA32_FMASK, (UINT64)(0x200ULL));

    /* set CS/SS and syscall selectors */
    HalX64WriteMsr(IA32_STAR, (0x13ULL << 48) | (0x08ULL << 32));

    /* set the syscall dispatcher routine */
    HalX64WriteMsr(IA32_LSTAR, (ULONG_PTR)HalpSystemCall);
}

VOID SetupSystemCallHandler(SYSCALL_HANDLER SystemRoutine)
{
    HalpSystemCallHandler = SystemRoutine;
}
