#ifndef NNX_OSDBG_H
#define NNX_OSDBG_H

#include "video/SimpleTextIO.h"
#include "nnxint.h"
#include "HAL/IDT.h"

inline void NNXAssert(BOOL x, char* s) {
	if (!x)
		PrintT("%s", s);
}

inline void NNXAssertAndStop(BOOL x, char* s) {
	NNXAssert(x, s);
	if (!x) {
		DisableInterrupts();
		while (true);
	}
}

#define SHOWCODEPOS PrintT("%i %s\n",__LINE__,__FUNCSIG__);

#endif
