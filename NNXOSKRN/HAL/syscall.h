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
        ULONG_PTR param4
    );

    SYSCALL_HANDLER SetupSystemCallHandler(SYSCALL_HANDLER SystemRoutine);

    VOID HalInitializeSystemCallForCurrentCore();

    ULONG_PTR SystemCallHandler(
        ULONG_PTR p1,
        ULONG_PTR p2,
        ULONG_PTR p3,
        ULONG_PTR p4
    );

#ifdef __cplusplus
}
#endif