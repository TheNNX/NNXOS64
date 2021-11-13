#ifndef NNX_BUGCHECK_HEADER
#define NNX_BUGCHECK_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif
	__declspec(noreturn) VOID KeBugCheck(ULONG code);
	__declspec(noreturn) VOID KeBugCheckEx(ULONG code, ULONG_PTR param1, ULONG_PTR param2, ULONG_PTR param3, ULONG_PTR param4);
#ifdef __cplusplus
}
#endif

#define BC_KMODE_EXCEPTION_NOT_HANDLED	0x1E
#define BC_PHASE1_INITIALIZATION_FAILED 0x32
#define BC_HAL_MEMORY_ALLOCATION		0xAC

#endif