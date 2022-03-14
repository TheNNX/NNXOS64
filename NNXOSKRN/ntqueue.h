/*
    defines KQUEUE and related functions
*/

#ifndef NNX_NT_QUEUE_HEADER
#define NNX_NT_QUEUE_HEADER

#include "HAL/cpu.h"
#include <ob/object.h>

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

    VOID KeInitializeQueue(PKQUEUE Queue, ULONG MaxmimumWaitingThreads);

    PLIST_ENTRY KeRemvoeQueue(PKQUEUE Queue, KPROCESSOR_MODE WaitMode, PLONG64 Timeout);

    LONG KeInsertHeadQueue(PKQUEUE Queue, PLIST_ENTRY Entry);
    
    LONG KeInsertQueue(PKQUEUE Queue, PLIST_ENTRY Entry);

    PLIST_ENTRY KeRundownQueue(PKQUEUE Queue);

#ifdef __cplusplus
}
#endif

#endif
