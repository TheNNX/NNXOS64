#ifndef NNX_PIC_HEADER
#define NNX_PIC_HEADER
#define PIC_PRIMARY 0x20
#define PIC_SECONDARY 0xA0

#define PIC_PRIMARY_DATA PIC_PRIMARY+1
#define PIC_SECONDARY_DATA PIC_SECONDARY+1

#include "../nnxint.h"

#ifdef __cplusplus
extern "C" {
#endif

	VOID PicInitialize();
	VOID PicDisableForApic();

#ifdef __cplusplus
}
#endif

#endif