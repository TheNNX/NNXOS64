#include "bugcheck.h"
#include <video/SimpleTextIO.h>

VOID KeBugCheck(ULONG code)
{
	KeBugCheckEx(code, NULL, NULL, NULL, NULL);
}

__declspec(noreturn) VOID KeStop();

VOID KeBugCheckEx(ULONG code, 
				  ULONG_PTR param1, ULONG_PTR param2, ULONG_PTR param3, ULONG_PTR param4)
{
	
	/* TODO: notify other CPUs */

	TextIoSetColorInformation(0xFFFFFFFF, 0xFF0000AA, TRUE);
	TextIoClear();
	TextIoSetCursorPosition(0, 8);

	PrintT("Critical system failure\n");

	/* TODO */
	if (code == BC_KMODE_EXCEPTION_NOT_HANDLED)
	{
		PrintT("KMODE_EXCEPTION_NOT_HANDLED");	
	}
	else if (code == BC_PHASE1_INITIALIZATION_FAILED)
	{
		PrintT("PHASE1_INITIALIZATION_FAILED");
	}
	else
	{
		PrintT("BUGCHECK_CODE_%X", code);
	}

	PrintT("\n\n");
	PrintT("0x%X, 0x%X, 0x%X, 0x%X\n\n\n", param1, param2, param3, param4);
	PrintT("Registers:\n"
		   "RAX 0x%H  RBX 0x%H  RCX 0x%H  RDX 0x%H\n"
		   "RDI 0x%H  RSI 0x%H  RSP 0x%H  RBP 0x%H\n"
		   "R8  0x%H  R9  0x%H  R10 0x%H  R11 0x%H\n"
		   "R12 0x%H  R13 0x%H  R14 0x%H  R15 0x%H\n"
		   "CR2 0x%H  CR3 0x%H  CR4 0x%H",
		   GetRAX(), GetRBX(), GetRCX(), GetRDX(),
		   GetRDI(), GetRSI(), GetRSP(), GetRBP(),
		   GetR8() , GetR9() , GetR10(), GetR11(),
		   GetR12(), GetR13(), GetR14(), GetR15(),
		   GetCR2(), GetCR3(), GetCR4()
	);

	KeStop();
}