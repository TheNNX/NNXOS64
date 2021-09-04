#ifndef NNX_OSDBG_H
#define NNX_OSDBG_H

#include "video/SimpleTextIo.h"
#include <nnxtype.h>
#include "HAL/IDT.h"

#ifdef __cplusplus
extern "C"{
#endif

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


#ifdef __cplusplus
}

inline void NNXAssert(BOOL x, const char* s) {
	NNXAssert(x, (char*)s);
}

inline void NNXAssertAndStop(BOOL x, const char* s) {
	NNXAssertAndStop(x, (char*)s);
}

#endif

#define SHOWCODEPOS PrintT("%i %s\n",__LINE__,__FUNCSIG__);

#endif
