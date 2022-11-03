#pragma once

#include <nnxtype.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef VOID(*SYSCALL_HANDLER)();
    VOID SetupSystemCallHandler(SYSCALL_HANDLER SystemRoutine);
    VOID HalInitializeSystemCallForCurrentCore();

#ifdef __cplusplus
}
#endif