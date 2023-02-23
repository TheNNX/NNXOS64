/*
    defines KQUEUE and related functions
*/

#ifndef NNX_NT_QUEUE_HEADER
#define NNX_NT_QUEUE_HEADER

#include "dispatcher.h"
#include "HAL/cpu.h"
#include <ob/object.h>
#include <scheduler.h>

#ifdef __cplusplus
extern "C" 
{
#endif

    typedef struct _KQUEUE
    {
        DISPATCHER_HEADER Header;
        LIST_ENTRY EntryListHead;
        ULONG CurrentWaitingThreads;
        ULONG MaximumWaitingThreads;
        LIST_ENTRY ThreadsHead;
    }KQUEUE, *PKQUEUE;

    VOID 
    NTAPI 
    KeInitializeQueue(PKQUEUE Queue, ULONG MaxmimumWaitingThreads);

    PLIST_ENTRY 
    NTAPI 
    KeRemoveQueue(PKQUEUE Queue, KPROCESSOR_MODE WaitMode, PLONG64 Timeout);

    LONG 
    NTAPI 
    KeInsertHeadQueue(PKQUEUE Queue, PLIST_ENTRY Entry);
    
    LONG 
    NTAPI 
    KeInsertQueue(PKQUEUE Queue, PLIST_ENTRY Entry);

    VOID
    NTAPI
    KiUnwaitWaitBlockFromQueue(
        PKWAIT_BLOCK pWaitBlock);

#ifdef __cplusplus
}
#endif

#endif
