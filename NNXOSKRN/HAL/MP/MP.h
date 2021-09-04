#ifndef NNX_MP_HEADER
#define NNX_MP_HEADER

#include <nnxtype.h>
#include "../../../NNXOSLDR/HAL/GDT.h"
#include "../../../NNXOSLDR/HAL/IDT.h"

#ifdef __cplusplus
extern "C"{
#endif
	VOID MpInitialize();
	VOID ApProcessorInit(UINT8 lapicID);

#pragma pack(push, 1)
	typedef struct ApData {
		UINT16 ApSpinlock;
		UINT8 Padding[62];
		UINT8 ApCurrentlyBeingInitialized;
		UINT64 ApCR3;
		PVOID* ApStackPointerArray;
		VOID(*ApProcessorInit)(UINT8 lapicId);
		GDTR ApGdtr;
		IDTR ApIdtr;
	}ApData;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#define AP_INITIAL_STACK_SIZE 512

#endif