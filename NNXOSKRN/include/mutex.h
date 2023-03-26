#pragma once

#include <dispatcher.h>

#ifdef __cplusplus
extern "C"{
#endif
    typedef struct _KMUTANT
    {
        DISPATCHER_HEADER Header;
        LIST_ENTRY        MutantListEntry;
        struct _KTHREAD*  Owner;
    }KMUTEX, *PKMUTEX, *KMUTANT, *PKMUTANT;

    NTSYSAPI
    LONG 
    NTAPI    
    KeReleaseMutex(
        PKMUTEX pMutex, 
        BOOLEAN Wait);

#ifdef NNX_KERNEL
    VOID
    NTAPI
    KiUnwaitWaitBlockMutex(
        PKWAIT_BLOCK pWaitBlock);
#endif

#ifdef __cplusplus
}
#endif