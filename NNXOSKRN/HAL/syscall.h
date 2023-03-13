#pragma once

#include <nnxtype.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef ULONG_PTR(*SYSCALL_HANDLER)(
        ULONG_PTR param1, 
        ULONG_PTR param2, 
        ULONG_PTR param3, 
        ULONG_PTR param4);

    NTSYSAPI
    SYSCALL_HANDLER
    NTAPI    
    SetupSystemCallHandler(
        SYSCALL_HANDLER SystemRoutine);

#ifdef NNX_KERNEL
    VOID 
    NTAPI    
    HalInitializeSystemCallForCurrentCore();

    ULONG_PTR 
    NTAPI    
    SystemCallHandler(
        ULONG_PTR p1,
        ULONG_PTR p2,
        ULONG_PTR p3,
        ULONG_PTR p4);
#endif

#ifdef __cplusplus
}
#endif