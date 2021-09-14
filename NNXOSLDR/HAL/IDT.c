#include "IDT.h"
#include "video/SimpleTextIo.h"
#include "device/Keyboard.h"
#include "registers.h"

BOOL gInteruptInitialized;

void ExceptionHandler(UINT64 n, UINT64 errcode, UINT64 rip) 
{
	PrintT("error: %x %x at RIP 0x%X\n\nRegisters:\nRAX %X  RBX %X  RCX %X  RDX %X\nRDI %X  RSI %X  RSP %X  RBP %X\nCR2 %X", n, errcode, rip,
		GetRAX(), GetRBX(), GetRCX(), GetRDX(),
		GetRDI(), GetRSI(), GetRSP(), GetRBP(),
		GetCR2()
	);
	while (1);
}

void IrqHandler(UINT32 n) 
{
	if (n == 1) 
	{
		UINT8 character = KeyboardInterrupt();
		switch (character) 
		{
		case 0:
			return;
		default:
			PrintT("%c",character);
			return;
			/* TODO: AddKeyToKeyboardBuffer(character); */
		};
	}
}

