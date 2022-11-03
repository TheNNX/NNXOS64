#ifndef NNX_CPU_X64_HEADER
#define NNX_CPU_X64_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum _MODE
	{
		KernelMode = 0,
		UserMode = 1
	}MODE;

	VOID HalSetPcr(struct _KPCR* pcr);

#include "registers.h"
#include <HAL/Port.h>

#ifdef __cplusplus
}
#endif

#endif