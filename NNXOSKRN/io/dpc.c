#include "dpc.h"

#include <ntdebug.h>
#include <pcr.h>
#include <SimpleTextIO.h>

NTSYSAPI 
VOID 
NTAPI 
KeInitializeDpc(PKDPC Dpc,
                PKDEFERRED_ROUTINE DeferredRoutine,
                PVOID DeferredContext)
{
    ASSERT(Dpc != NULL);

    Dpc->Routine = DeferredRoutine;
    Dpc->Context = DeferredContext;
    if (Dpc->DpcData != NULL)
    {
        PrintT("Warning: DpcData not null\n", Dpc->DpcData);
    }
    Dpc->DpcData = &KeGetPcr()->Prcb->DpcData;
    Dpc->Inserted = FALSE;
}

NTSYSAPI 
BOOLEAN 
NTAPI 
KeInsertQueueDpc(PKDPC Dpc,
                 PVOID SystemArgument1,
                 PVOID SystemArgument2)
{
    PKDPC_DATA data;
    KIRQL irql;
    
    KeRaiseIrql(HIGH_LEVEL, &irql);
    data = Dpc->DpcData;

    KiAcquireSpinLock(&data->DpcLock);

    if (Dpc->Inserted)
    {
        KeReleaseSpinLock(&data->DpcLock, irql);
        return FALSE;
    }

    Dpc->SystemArgument1 = SystemArgument1;
    Dpc->SystemArgument2 = SystemArgument2;
    Dpc->Inserted = TRUE;
    InsertTailList(&data->DpcListHead, &Dpc->Entry);
    data->DpcQueueDepth++;
    data->DpcCount++;

    KeReleaseSpinLock(&data->DpcLock, irql);
    return TRUE;
}