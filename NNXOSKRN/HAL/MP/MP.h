#ifndef NNX_MP_HEADER
#define NNX_MP_HEADER

#include "../../../NNXOSLDR/nnxint.h"

#ifdef __cplusplus
extern "C"{
#endif
	VOID MpInitialize();

#pragma pack(push, 1)
	typedef struct ApData {
		UINT16 ApSpinlock;
		UINT8 Padding[62];
		UINT8 ApCurrentlyBeingInitialized;
		UINT16 ApGdtrPointer;
		UINT16 ApIdtrPointer;
		UINT64 ApCR3;
		UINT16 ApStackPointer16;
		UINT64 ApStackPointer64;
	}ApData;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif


#endif