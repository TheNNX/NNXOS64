#pragma once
#include <spinlock.h>
#include <cpu.h>
#include <ntlist.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum KWAIT_REASON
    {
        Executive = 0
    }KWAIT_REASON;

    typedef enum _WAIT_TYPE
    {
        WaitAll,
        WaitAny
    }WAIT_TYPE;

    typedef enum _KOBJECTS
    {
        ProcessObject,
        ThreadObject,
        QueueObject,
        EventObject,
        TimerObject,
        MutexObject
    }KOBJECTS;

    typedef struct _DISPATCHER_HEADER
    {
        union
        {
            KSPIN_LOCK Lock;
            struct
            {
                BYTE Padding[3];
                BYTE Type;
            };
        };
        LONG SignalState;
        LIST_ENTRY WaitHead;
    }DISPATCHER_HEADER, *PDISPATCHER_HEADER;

    typedef struct _KWAIT_BLOCK
    {
        LIST_ENTRY WaitEntry;
        struct _KTHREAD* Thread;
        DISPATCHER_HEADER* Object;
        UCHAR WaitType;
        KPROCESSOR_MODE WaitMode;
    }KWAIT_BLOCK, *PKWAIT_BLOCK;

    typedef struct _KTIMEOUT_ENTRY
    {
        LIST_ENTRY ListEntry;
        /* In hundreds of nanoseconds;
         * if absolute, since 1st of January, 1601 */
        ULONG64 Timeout;
        BOOLEAN TimeoutIsAbsolute;
        VOID(NTAPI*OnTimeout)(struct _KTIMEOUT_ENTRY* TimeoutEntry);
    }KTIMEOUT_ENTRY, *PKTIMEOUT_ENTRY;

    inline 
    VOID 
    InitializeDispatcherHeader(
        DISPATCHER_HEADER* Header, 
        UCHAR Type)
    {
        KeInitializeSpinLock(&Header->Lock);
        Header->Type = Type;
        Header->SignalState = 0;
        InitializeListHead(&Header->WaitHead);
    }

    NTSYSAPI
    NTSTATUS
    NTAPI
    KeWaitForSingleObject(
        PVOID Object,
        KWAIT_REASON WaitReason,
        KPROCESSOR_MODE WaitMode,
        BOOLEAN Alertable,
        PLONG64 Timeout
    );

    NTSYSAPI
    NTSTATUS
    NTAPI
    KeWaitForMultipleObjects(
        ULONG Count,
        PVOID *Object,
        WAIT_TYPE WaitType,
        KWAIT_REASON WaitReason,
        KPROCESSOR_MODE WaitMode,
        BOOLEAN Alertable,
        PLONG64 Timeout,
        PKWAIT_BLOCK WaitBlockArray
    );

    NTSYSAPI
    KIRQL
    NTAPI
    KiAcquireDispatcherLock();

    NTSYSAPI
    VOID
    NTAPI
    KiReleaseDispatcherLock(KIRQL oldIrql);

#ifdef NNX_KERNEL

    extern KSPIN_LOCK DispatcherLock;

    NTSTATUS
    NTAPI
    KeInitializeDispatcher();

    VOID
    NTAPI
    KeUnwaitThread(
        struct _KTHREAD* pThread,
        LONG_PTR WaitStatus,
        LONG PriorityIncrement);

    VOID
    NTAPI
    KeUnwaitThreadNoLock(
        struct _KTHREAD* pThread,
        LONG_PTR WaitStatus,
        LONG PriorityIncrement);

    VOID
    NTAPI
    KiHandleObjectWaitTimeout(
        struct _KTHREAD* Thread, 
        PLONG64 pTimeout);

    VOID
    NTAPI
    KiUnwaitWaitBlock(
        PKWAIT_BLOCK pWaitBlock,
        BOOLEAN Designal,
        LONG_PTR WaitStatus,
        LONG PriorityIncrement);

    VOID
    NTAPI
    KiSignal(
        PDISPATCHER_HEADER Object,
        ULONG SignalIncrememnt);

    VOID
    NTAPI
    KiClockTick();
#endif

#ifdef __cplusplus
}
#endif
