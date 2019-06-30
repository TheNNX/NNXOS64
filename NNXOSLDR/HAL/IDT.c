#include "IDT.h"
#include "video/SimpleTextIO.h"
#include "device/Keyboard.h"

void ExceptionHandler(UINT32 n, UINT32 errcode) {
	if (n == 0xe)
		PrintT("pgf: %x ",GetCR2());
	PrintT("%x %x\n",(UINT64)n,(UINT64)errcode);
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

