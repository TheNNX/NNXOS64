#pragma once
#include "dispatcher.h"

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct _KSEMAPHORE
    {
        DISPATCHER_HEADER Header;
        LONG Limit;
    } *PKSEMAPHORE, KSEMAPHORE;

    NTSYSAPI VOID NTAPI KeInitializeSemaphore(PKSEMAPHORE Semaphore, 
                                              LONG Count, 
                                              LONG Limit);

    NTSYSAPI LONG NTAPI KeReleaseSemaphore(PKSEMAPHORE Semaphore,
                                           LONG Increment,
                                           LONG Adjustment,
                                           BOOLEAN Wait);

    NTSYSAPI LONG NTAPI KeReadStateSemaphore(PKSEMAPHORE Semaphore);

#ifdef NNX_KERNEL
    VOID NTAPI KiUnwaitWaitBlockFromSemaphore(PKWAIT_BLOCK pWaitBlock);
#endif

#ifdef __cplusplus
}
#endif