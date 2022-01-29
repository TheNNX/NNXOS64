#ifndef NNX_REGISTERS_HEADER
#define NNX_REGISTERS_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C"
{
#endif

	UINT64 GetRAX();
	UINT64 GetRBX();
	UINT64 GetRCX();
	UINT64 GetRDX();
	UINT64 GetRDI();
	UINT64 GetRSI();
	UINT64 GetRBP();
	UINT64 GetRSP();
	UINT64 GetR8();
	UINT64 GetR9();
	UINT64 GetR10();
	UINT64 GetR11();
	UINT64 GetR12();
	UINT64 GetR13();
	UINT64 GetR14();
	UINT64 GetR15();
	UINT64 GetCR0();
	UINT64 GetCR2();
	UINT64 GetCR3();
	UINT64 GetCR4();
	UINT64 GetCR8();
	VOID SetCR3(UINT64);
	VOID SetCR4(UINT64);
	VOID SetCR0(UINT64);
	VOID SetCR8(UINT64);
	VOID HalX64WriteMsr(UINT32 reg, UINT64 value);
	PVOID HalX64SwapGs();

#ifdef __cplusplus
}
#endif

#endif