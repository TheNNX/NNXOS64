#pragma once
#include <HAL/spinlock.h>
#include <HAL/cpu.h>
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
        KPROCESSOR_MODE WaitMode;
        UCHAR WaitType;
        struct _KTHREAD* Thread;
        DISPATCHER_HEADER* Object;
    }KWAIT_BLOCK, *PKWAIT_BLOCK;

    inline 
    VOID 
    InitializeDispatcherHeader(DISPATCHER_HEADER* Header, UCHAR Type)
    {
        KeInitializeSpinLock(&Header->Lock);
        Header->Type = Type;
        Header->SignalState = 0;
        InitializeListHead(&Header->WaitHead);
    }

    NTSTATUS
    NTAPI
    KeWaitForSingleObject(
        PVOID Object,
        KWAIT_REASON WaitReason,
        KPROCESSOR_MODE WaitMode,
        BOOL Alertable,
        PLONG64 Timeout
    );

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
        LONG PriorityIncrement
    );

    VOID
    NTAPI
    KiHandleObjectWaitTimeout(struct _KTHREAD* Thread, PLONG64 pTimeout, BOOL Alertable);

    extern KSPIN_LOCK DispatcherLock;

    KIRQL
    NTAPI
    KiAcquireDispatcherLock();

    VOID
    NTAPI 
    KiReleaseDispatcherLock(KIRQL oldIrql);

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

#ifdef __cplusplus
}
#endif
