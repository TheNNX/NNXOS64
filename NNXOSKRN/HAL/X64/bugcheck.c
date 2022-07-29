#include "bugcheck.h"
#include <SimpleTextIO.h>
#include "IDT.h"
#include "APIC.h"
#include "registers.h"
#include <HAL/spinlock.h>
#include <scheduler.h>

VOID KeBugCheck(ULONG code)
{
	KeBugCheckEx(code, NULL, NULL, NULL, NULL);
}

__declspec(noreturn) VOID KeStop();

static KSPIN_LOCK BugcheckLock = 0;

__declspec(noreturn)
VOID
KeBugCheckEx(
	ULONG code,
	ULONG_PTR param1,
	ULONG_PTR param2,
	ULONG_PTR param3,
	ULONG_PTR param4
)
{
	KIRQL irql;
	DisableInterrupts();
	KeAcquireSpinLock(&BugcheckLock, &irql);

	/* TODO: notify other CPUs */
	if (FALSE)
	{
		TextIoSetColorInformation(0xFFFFFFFF, 0xFF0000AA, TRUE);
		TextIoClear();
		TextIoSetCursorPosition(0, 8);
	}

	PrintT("\nCore %i, thread %X\n\nCritical system failure\n", ApicGetCurrentLapicId(), KeGetCurrentThread());

	/* TODO */
	if (code == KMODE_EXCEPTION_NOT_HANDLED)
	{
		PrintT("KMODE_EXCEPTION_NOT_HANDLED");	
	}
	else if (code == PHASE1_INITIALIZATION_FAILED)
	{
		PrintT("PHASE1_INITIALIZATION_FAILED");
	}
	else if (code == HAL_INITIALIZATION_FAILED)
	{
		PrintT("HAL_INITIALIZATION_FAILED");
	}
	else
	{
		PrintT("BUGCHECK_CODE_%X", code);
	}

	PrintT("\n\n");
	PrintT("0x%X, 0x%X, 0x%X, 0x%X\n\n\n", param1, param2, param3, param4);
	PrintT("CR2: %H CR3: %H", GetCR2(), GetCR3());

	KeReleaseSpinLock(&BugcheckLock, irql);

	KeStop();
}