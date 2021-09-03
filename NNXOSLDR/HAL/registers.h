#ifndef NNX_REGISTERS_HEADER
#define NNX_REGISTERS_HEADER

#include <nnxint.h>

#ifdef __cplusplus
extern "C" {
#endif

	UINT64 GetRAX();
	UINT64 GetRBX();
	UINT64 GetRCX();
	UINT64 GetRDX();
	UINT64 GetRDI();
	UINT64 GetRSI();
	UINT64 GetRBP();
	UINT64 GetRSP();
	UINT64 GetCR0();
	UINT64 GetCR2();
	UINT64 GetCR3();
	UINT64 GetCR4();
	VOID SetCR3(UINT64);
	VOID SetCR4(UINT64);
	VOID SetCR0(UINT64);

#ifdef __cplusplus
}
#endif

#endif