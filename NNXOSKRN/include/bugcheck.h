#ifndef NNX_BUGCHECK_HEADER
#define NNX_BUGCHECK_HEADER

#include <nnxtype.h>
#include <interrupt.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NNX_KERNEL
    __declspec(noreturn) 
    BOOLEAN 
    NTAPI 
    KeStopIsr(
        PKINTERRUPT, 
        PVOID);

#endif

    NTHALAPI
    __declspec(noreturn)
    VOID
    NTAPI
    KeStop();

    __declspec(noreturn) 
    NTSYSAPI 
    VOID 
    NTAPI 
    KeBugCheck(
        ULONG code);

    __declspec(noreturn) 
    NTSYSAPI 
    VOID 
    NTAPI
    KeBugCheckEx(
        ULONG code, 
        ULONG_PTR param1, 
        ULONG_PTR param2, 
        ULONG_PTR param3, 
        ULONG_PTR param4);

#ifdef __cplusplus
}
#endif

#define IRQL_NOT_DISPATCH_LEVEL         ((ULONG)0x8)
#define IRQL_NOT_GREATER_OR_EQUAL       ((ULONG)0x9)
#define IRQL_NOT_LESS_OR_EQUAL          ((ULONG)0xA)
#define SPIN_LOCK_NOT_OWNED             ((ULONG)0x10)
#define BAD_POOL_HEADER                 ((ULONG)0x19)
#define KMODE_EXCEPTION_NOT_HANDLED     ((ULONG)0x1E)
#define PHASE1_INITIALIZATION_FAILED    ((ULONG)0x32)
#define PAGE_FAULT_WITH_INTERRUPTS_OFF  ((ULONG)0x49)
#define PAGE_FAULT_IN_NONPAGED_AREA     ((ULONG)0x50)
#define HAL_INITIALIZATION_FAILED       ((ULONG)0x5C)
#define HAL_MEMORY_ALLOCATION           ((ULONG)0xAC)
#define CRITICAL_PROCESS_DIED           ((ULONG)0xEF)
#define BAD_POOL_CALLER                 ((ULONG)0xC2)
#define CRITICAL_STRUCTURE_CORRUPTION   ((ULONG)0x109)
#define WORKER_THREAD_TEST_CONDITION    ((ULONG)0x163)
#define MANUALLY_INITIATED_CRASH1       ((ULONG)0xDEADDEAD)

#endif