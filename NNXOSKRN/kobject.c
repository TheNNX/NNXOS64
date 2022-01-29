#include "object.h"
#include <scheduler.h>
#include <pool.h>

POBJECT_TYPE ObjTypeObjType = NULL;
LIST_ENTRY ObObjectTypes;

NTSTATUS ObInitialize()
{
    InitializeListHead(&ObObjectTypes);


    return STATUS_SUCCESS;
}

VOID KiHandleObjectWaitTimeout(PKTHREAD Thread, PLONG64 pTimeout, BOOL Alertable)
{
    /* if Timeout != NULL && *Timeout == 0 is handled earlier */
    if (pTimeout == NULL)
    {
        Thread->TimeoutIsAbsolute = TRUE;
        Thread->Timeout = UINT64_MAX;
    }
    else
    {
        Thread->TimeoutIsAbsolute = TRUE;

        if (*pTimeout < 0)
        {
            Thread->TimeoutIsAbsolute = FALSE;
            *pTimeout = -*pTimeout;
        }

        Thread->Timeout = *pTimeout;
    }

    Thread->Alertable = Alertable;
}

NTSTATUS KeWaitForMultipleObjects(
    ULONG Count,
    PVOID *Objects,
    WAIT_TYPE WaitType,
    KWAIT_REASON WaitReason,
    KPROCESSOR_MODE WaitMode,
    BOOLEAN Alertable,
    PLONG64 pTimeout,
    PKWAIT_BLOCK WaitBlockArray
)
{
    ULONG i;
    BOOL done = FALSE;
    ULONG ready = 0;
    KIRQL originalIrql;

    if (Count > THREAD_WAIT_OBJECTS && WaitBlockArray == NULL)
        return STATUS_INVALID_PARAMETER;

    KeRaiseIrql(DISPATCH_LEVEL, &originalIrql);

    /* lock all objects */
    for (i = 0; i < Count; i++)
    {
        DISPATCHER_HEADER* dispHeader = (DISPATCHER_HEADER*)Objects[i];
        KeAcquireSpinLockAtDpcLevel(&dispHeader->Lock);
    }

    for (i = 0; i < Count; i++)
    {
        DISPATCHER_HEADER* dispHeader = (DISPATCHER_HEADER*)Objects[i];
        ready += (dispHeader->SignalState != 0);
    }

    done |= (WaitType == WaitAll && ready == Count);
    done |= (WaitType == WaitAny && ready > 0);

    if (done)
    {
        for (i = 0; i < Count; i++)
        {
            DISPATCHER_HEADER* dispHeader = (DISPATCHER_HEADER*)Objects[i];

            if (dispHeader->SignalState && WaitType == WaitAny)
            {
                dispHeader->SignalState--;
                break;
            }
            /* if Done is true, all objects are signaling */
            else if (WaitType == WaitAll)
            {
                dispHeader->SignalState--;
            }
        }
    }

    /* release all locks */
    for (i = 0; i < Count; i++)
    {
        DISPATCHER_HEADER* dispHeader = (DISPATCHER_HEADER*)Objects[i];
        KeReleaseSpinLockFromDpcLevel(&dispHeader->Lock);
    }

    /* timeout of lenght 0 and yet, none of the objects were signaling */
    if (pTimeout != NULL && *pTimeout == 0)
    {
        KeLowerIrql(originalIrql);
        return STATUS_TIMEOUT;
    }

    /* waiting is neccessary */
    if (!done)
    {
        PKTHREAD currentThread = PspGetCurrentThread();
        ULONG i;
        
        /* this will elevate us to IRQL = DISPATCH_LEVEL */
        KeAcquireSpinLockAtDpcLevel(&currentThread->ThreadLock);
        
        if (Count > THREAD_WAIT_OBJECTS)
        {
            currentThread->NumberOfCustomThreadWaitBlocks = Count;
            currentThread->CustomThreadWaitBlocks = WaitBlockArray;
        }
        else
        {
            WaitBlockArray = currentThread->ThreadWaitBlocks;
        }

        InitializeListHead((PLIST_ENTRY)&currentThread->WaitHead);
        for (i = 0; i < Count; i++)
        {
            WaitBlockArray[i].Object = (DISPATCHER_HEADER*)Objects[i];
            WaitBlockArray[i].WaitMode = WaitMode;
            WaitBlockArray[i].WaitType = WaitType;
            InsertTailList((PLIST_ENTRY)&currentThread->WaitHead, &WaitBlockArray[i].WaitEntry);
        }

        KiHandleObjectWaitTimeout(currentThread, pTimeout, Alertable);

        KeReleaseSpinLock(&currentThread->ThreadLock, originalIrql);
        PspSchedulerNext();
    }
    else
    {
        KeLowerIrql(originalIrql);
    }

    return STATUS_SUCCESS;
}

NTSTATUS KeWaitForSingleObject(PVOID Object, KWAIT_REASON WaitReason, KPROCESSOR_MODE WaitMode, BOOL Alertable, PLONG64 Timeout)
{
    return KeWaitForMultipleObjects(1, &Object, WaitAll, WaitReason, WaitMode, Alertable, Timeout, NULL);
}

inline POBJECT_HEADER ObpGetObjectHeaderFromObject(PVOID Object)
{
    return (POBJECT_HEADER)((ULONG_PTR)Object - sizeof(OBJECT_HEADER));
}

VOID NTAPI ObReferenceObject(
    PVOID Object
)
{
    KIRQL irql;
    POBJECT_HEADER objectHeader = ObpGetObjectHeaderFromObject(Object);

    /* just to be sure */
    KeAcquireSpinLock(&objectHeader->Lock, &irql);

    objectHeader->ReferenceCount++;

    KeReleaseSpinLock(&objectHeader->Lock, irql);
}

NTSTATUS NTAPI ObReferenceObjectByPointer(
    PVOID Object,
    ACCESS_MASK AccessMask,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode
)
{
    POBJECT_HEADER objectHeader = ObpGetObjectHeaderFromObject(Object);

    if (AccessMode == UserMode || 
        objectHeader->Attributes.Attributes & OBJ_FORCE_ACCESS_CHECK
        )
    {
        if (objectHeader->ObjectType != ObjectType)
            return STATUS_OBJECT_TYPE_MISMATCH;
    }

    ObReferenceObject(Object);

    return STATUS_SUCCESS;
}

NTSTATUS NTAPI ObReferenceObjectByHandle(
    HANDLE ObjectHandle,
    ACCESS_MASK AccessMask,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    PVOID* pObject,
    PVOID HandleInformation
)
{
    POBJECT_HEADER objectHeader = ObpGetObjectHeaderFromObject(ObjectHandle->Object);
 
    if (AccessMode == UserMode || 
        objectHeader->Attributes.Attributes & OBJ_FORCE_ACCESS_CHECK
        )
    {
        if (objectHeader->ObjectType != ObjectType)
            return STATUS_OBJECT_TYPE_MISMATCH;
    }

    if (pObject != NULL)
        *pObject = ObjectHandle->Object;

    return STATUS_SUCCESS;
}

VOID NTAPI ObDereferenceObject(PVOID Object)
{
    KIRQL irql;
    POBJECT_HEADER objectHeader = ObpGetObjectHeaderFromObject(Object);

    KeAcquireSpinLock(&objectHeader->Lock, &irql);

    objectHeader->ReferenceCount++;

    KeReleaseSpinLock(&objectHeader->Lock, irql);

    if (ObpCheckObjectForDeletion(Object))
        ObpDeleteObject(Object);
}


BOOL ObpCheckObjectForDeletion(PVOID Object)
{
    POBJECT_HEADER objectHeader = ObpGetObjectHeaderFromObject(Object);

    if (objectHeader->Attributes.Attributes & OBJ_PERMANENT)
        return FALSE;

    return (objectHeader->ReferenceCount == 0 && objectHeader->HandleCount == 0);
}

VOID ObpDeleteName(PVOID Object)
{
    KIRQL irql;
    POBJECT_HEADER objectHeader = ObpGetObjectHeaderFromObject(Object);
    
    KeAcquireSpinLock(&objectHeader->Lock, &irql);

    /* handle count could have grown between the last check and now */
    if (objectHeader->HandleCount == 0)
    {
        if (objectHeader->Attributes.ObjectName != NULL)
        {
            ExFreePool(objectHeader->Attributes.ObjectName);
            objectHeader->Attributes.ObjectName = NULL;
        }
    }

    KeReleaseSpinLock(&objectHeader->Lock, irql);
}

NTSTATUS ZwClose(HANDLE Handle)
{
    if (--ObpGetObjectHeaderFromObject(Handle->Object)->HandleCount == 0)
    {
        ObpDeleteName(Handle->Object);
    }
    ExFreePool((PVOID)Handle);

    return STATUS_SUCCESS;
}

VOID ObpDeleteObject(PVOID Object)
{
    KIRQL irql;
    POBJECT_HEADER objectHeader = ObpGetObjectHeaderFromObject(Object);

    KeAcquireSpinLock(&objectHeader->Lock, &irql);

    if (ObpCheckObjectForDeletion(Object) == FALSE)
    {
        KeReleaseSpinLock(&objectHeader->Lock, irql);
        return;
    }

    ObpDeleteName(Object);

    ExFreePool(Object);

    KeReleaseSpinLock(&objectHeader->Lock, irql);
    ExFreePool(objectHeader);

}

NTSTATUS NTAPI ZwMakeTemporaryObject(HANDLE ObjHandle)
{
    KIRQL irql;
    POBJECT_HEADER objectHeader = ObpGetObjectHeaderFromObject(ObjHandle);

    KeAcquireSpinLock(&objectHeader->Lock, &irql);

    objectHeader->Attributes.Attributes &= ~(OBJ_PERMANENT);

    KeReleaseSpinLock(&objectHeader->Lock, irql);

    return STATUS_SUCCESS;
}

NTSTATUS ObpCreateObject(
    KPROCESSOR_MODE Unused1,
    POBJECT_TYPE ObjectType,
    POBJECT_ATTRIBUTES Attributes,
    KPROCESSOR_MODE AccessMode,
    PVOID Unused2,
    ULONG ObjectSize,
    ULONG PagedPoolCharge,
    ULONG NonPagedPoolCharge,
    PVOID* Object
)
{
    POBJECT_HEADER objectHeader;
    KIRQL irql;

    objectHeader = ExAllocatePool(NonPagedPool, sizeof(*objectHeader));

    KeInitializeSpinLock(&objectHeader->Lock);
    KeAcquireSpinLock(&objectHeader->Lock, &irql);

    objectHeader->Attributes = *Attributes;
    objectHeader->HandleCount = 0;
    objectHeader->ReferenceCount = 0;

    InitializeListHead((PLIST_ENTRY)&objectHeader->Children);

    KeReleaseSpinLock(&objectHeader->Lock, irql);

    return STATUS_SUCCESS;
}

NTSTATUS ObpInsertObject(PVOID Object)
{
    return STATUS_SUCCESS;
}
