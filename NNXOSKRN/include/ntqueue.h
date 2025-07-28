/*
    defines KQUEUE and related functions
*/

#ifndef NNX_NT_QUEUE_HEADER
#define NNX_NT_QUEUE_HEADER

#include <dispatcher.h>
#include <cpu.h>
#include <object.h>
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

    NTSYSAPI
    VOID 
    NTAPI 
    KeInitializeQueue(PKQUEUE Queue, ULONG MaxmimumWaitingThreads);

    NTSYSAPI
    PLIST_ENTRY 
    NTAPI 
    KeRemoveQueue(PKQUEUE Queue, KPROCESSOR_MODE WaitMode, PLONG64 Timeout);

    NTSYSAPI
    LONG 
    NTAPI 
    KeInsertHeadQueue(PKQUEUE Queue, PLIST_ENTRY Entry);
    
    NTSYSAPI
    LONG 
    NTAPI 
    KeInsertQueue(PKQUEUE Queue, PLIST_ENTRY Entry);

#ifdef NNX_KERNEL
    VOID
    NTAPI
    KiUnwaitWaitBlockFromQueue(
        PKWAIT_BLOCK pWaitBlock);
#endif

#ifdef __cplusplus
}
#endif

#endif
