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

static KSPIN_LOCK SystemCallSpinLock;
SYSCALL_HANDLER HalpSystemCallHandler;
VOID HalpSystemCall();

static BOOLEAN QueueInitialized = FALSE;
static KQUEUE Queue;
static KSPIN_LOCK SystemCallSpinLock = 0;


/**
 * @brief This is an architecture dependent implementation of the function
 * for system call initialization. It sets up all the neccessary registers
 * for the system call to be callable.
 */
VOID
NTAPI
HalInitializeSystemCallForCurrentCore(ULONG_PTR SyscallStub)
{
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
	ULONG_PTR p1,
	ULONG_PTR p2,
	ULONG_PTR p3,
	ULONG_PTR p4)
{
	ULONG_PTR result = 0;
	LONG64 timeout = -10000000;
	ULONG threadId = (ULONG_PTR)KeGetCurrentThread() & 0xFFFF;

	switch (p1)
	{
	case 1:
	case 2:
		PrintT("s%X%i ", threadId, KeGetCurrentIrql());
		KeWaitForSingleObject(&Queue, Executive, KernelMode, FALSE, &timeout);
		break;
	default:
		PrintT("Warning: unsupported system call %X(%X,%X,%X)!\n", p1, p2, p3, p4);
		ASSERT(FALSE);
	}

	return result;
}