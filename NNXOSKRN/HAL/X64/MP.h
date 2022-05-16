#ifndef NNX_MP_X64_HEADER
#define NNX_MP_X64_HEADER

#include <HAL/cpu.h>
#include <nnxtype.h>
#include <HAL/X64/GDT.h>
#include <HAL/X64/IDT.h>

#ifdef __cplusplus
extern "C" {
#endif
	VOID MpInitialize();
	VOID ApProcessorInit(UINT8 lapicID);

#pragma pack(push, 1)
	typedef struct _AP_DATA
	{
		UINT16 ApSpinlock;
		UINT8 Padding[62];
		UINT8 ApCurrentlyBeingInitialized;
		UINT64 ApCR3;
		PVOID* ApStackPointerArray;
		VOID(*ApProcessorInit)(UINT8 lapicId);
		KGDTR64 ApGdtr;
		_KIDTR64 ApIdtr;
	}AP_DATA;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#define AP_INITIAL_STACK_SIZE 512

#endif