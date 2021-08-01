#include "IDT.h"
#include "video/SimpleTextIO.h"
#include "device/Keyboard.h"
#include "registers.h"

void ExceptionHandler(UINT32 n, UINT32 errcode) {
	if (n == 0xe)
		PrintT("page fault, CR2: %x\n",GetCR2());
	PrintT("error: %x %x\n\nRegisters:\nRAX %X  RBX %X  RCX %X  RDX %X\nRDI %X  RSI %X  RSP %X  RBP %X\n",(UINT64)n,(UINT64)errcode, 
		GetRAX(), GetRBX(), GetRCX(), GetRDX(), 
		GetRDI(), GetRSI(), GetRSP(), GetRBP()
	);
	while (1);
}

void IRQHandler(UINT32 n) {
	if (n == 1) {
		UINT8 character = KeyboardInterrupt();
		switch (character) {
		case 0:
			return;
		default:
			PrintT("%c",character);
			return;
			/* TODO: AddKeyToKeyboardBuffer(character); */
		};
	}
}

