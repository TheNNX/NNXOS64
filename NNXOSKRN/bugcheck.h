#ifndef NNX_BUGCHECK_HEADER
#define NNX_BUGCHECK_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif
	__declspec(noreturn) VOID NTAPI KeBugCheck(ULONG code);
	__declspec(noreturn) VOID NTAPI KeBugCheckEx(ULONG code, ULONG_PTR param1, ULONG_PTR param2, ULONG_PTR param3, ULONG_PTR param4);
#ifdef __cplusplus
}
#endif

#define IRQL_NOT_DISPATCH_LEVEL         ((ULONG)0x8)
#define IRQL_NOT_GREATER_OR_EQUAL		((ULONG)0x9)
#define IRQL_NOT_LESS_OR_EQUAL			((ULONG)0xA)
#define SPIN_LOCK_NOT_OWNED				((ULONG)0x10)
#define BAD_POOL_HEADER					((ULONG)0x19)
#define KMODE_EXCEPTION_NOT_HANDLED		((ULONG)0x1E)
#define PHASE1_INITIALIZATION_FAILED	((ULONG)0x32)
#define HAL_INITIALIZATION_FAILED		((ULONG)0x5C)
#define HAL_MEMORY_ALLOCATION			((ULONG)0xAC)
#define BAD_POOL_CALLER					((ULONG)0xC2)
#define CRITICAL_STRUCTURE_CORRUPTION   ((ULONG)0x109)
#define WORKER_THREAD_TEST_CONDITION	((ULONG)0x163)
#define MANUALLY_INITIATED_CRASH1		((ULONG)0xDEADDEAD)

#endif