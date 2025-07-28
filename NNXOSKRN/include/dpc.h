#pragma once

#include <nnxtype.h>
#include <ntlist.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef VOID (*PKDEFERRED_ROUTINE)(struct _KDPC* Dpc,
                                       PVOID DeferredCtx,
                                       PVOID SystemArgument1,
                                       PVOID SystemArgument2);

    typedef struct _KDCP_DATA
    {
        LIST_ENTRY DpcListHead;
        KSPIN_LOCK DpcLock;
        ULONG      DpcQueueDepth;
        ULONG      DpcCount;
    } *PKDPC_DATA, KDPC_DATA;

    typedef struct _KDPC
    {
        BOOLEAN Inserted;
        LIST_ENTRY Entry;
        PKDEFERRED_ROUTINE Routine;
        PVOID Context;
        PVOID SystemArgument1;
        PVOID SystemArgument2;
        PKDPC_DATA DpcData;
    } *PKDPC, KDPC;

    NTSYSAPI VOID NTAPI KeInitializeDpc(PKDPC Dpc,
                                        PKDEFERRED_ROUTINE DeferredRoutine,
                                        PVOID DeferredContext);

    NTSYSAPI BOOLEAN NTAPI KeInsertQueueDpc(PKDPC Dpc,
                                            PVOID SystemArgument1,
                                            PVOID SystemArgument2);

#ifdef __cplusplus
}
#endif