#include <HAL/syscall.h>
#include <HAL/spinlock.h>
#include "cpu.h"
#include "GDT.h"
#include "IDT.h"
#include <scheduler.h>

static KSPIN_LOCK SystemCallSpinLock;

SYSCALL_HANDLER HalpSystemCallHandler;
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

/**
 * @brief Sets the system call handler.
 * @return Pointer to the previous system call handler.
 */
SYSCALL_HANDLER SetupSystemCallHandler(SYSCALL_HANDLER SystemRoutine)
{
	SYSCALL_HANDLER oldHandler = HalpSystemCallHandler;
	HalpSystemCallHandler = SystemRoutine;
	return oldHandler;
}

/**
 * @brief The default system call handler - note: it is possible
 * to use different handlers, and even to chain load them - to 
 * do so, set a different handler with SetupSystemCallHandler,
 * preserve the old handler, and call the old handler from within
 * the new handler. 
 */
ULONG_PTR SystemCallHandler(
	ULONG_PTR p1,
	ULONG_PTR p2,
	ULONG_PTR p3,
	ULONG_PTR p4
)
{
	ULONG_PTR result = 0;

	DisableInterrupts();
	switch (p1)
	{
	case 1:
	case 2:
		result = (ULONG_PTR)KeGetCurrentThread();
		PrintT("%X ", result & 0xFFFF);
		if (result != p2)
			PrintT("%X %X %X %X %X\n", result, p1, p2, p3, p4);
		break;
	default:
		PrintT("Warning: unsupported system call %X(%X,%X,%X)!\n", p1, p2, p3, p4);
	}
	EnableInterrupts();

	return result;
}
