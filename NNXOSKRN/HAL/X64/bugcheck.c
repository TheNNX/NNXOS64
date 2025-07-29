#include "bugcheck.h"
#include <SimpleTextIO.h>
#include <spinlock.h>
#include <scheduler.h>
#include <dispatcher.h>
#include <cpu.h>
#include <HALX64/include/APIC.h>
#include <interrupt.h>
#include <pool.h>
#include <pcr.h>

__declspec(noreturn)
VOID
NTAPI
KeBugCheck(ULONG code)
{
    KeBugCheckEx(code, 0, 0, 0, 0);
}

volatile long AtomicStopVariable = 0;

VOID
NTAPI
KeStopOtherCores()
{
    if (_InterlockedCompareExchange(&AtomicStopVariable, 1, 0) == 0)
    {
        KeSendIpi(KAFFINITY_ALL, STOP_IPI_VECTOR);
    }
    else
    {
        KeStop();
    }
}

#pragma warning(push)
/* Disable MSVC warning C4646: 
 * A function marked with the noreturn __declspec modifier 
 * should have a void return type. 
 *
 * This function should match the KSERVICE_ROUTINE signature. */
#pragma warning(disable: 4646)
__declspec(noreturn)
BOOLEAN
NTAPI
KeStopIsr(
    PKINTERRUPT StopInterrupt,
    PVOID ServiceContext)
{
    KeStop();
}
#pragma warning(pop)

__declspec(noreturn)
VOID
KeBugCheckEx(
    ULONG code,
    ULONG_PTR param1,
    ULONG_PTR param2,
    ULONG_PTR param3,
    ULONG_PTR param4)
{
    PrintT("%i trying bugcheck\n", KeGetCurrentProcessorId());
    KeStopOtherCores();
    HalDisableInterrupts();
    TextIoSetColorInformation(0xFFFFFFFF, 0xFF0000AA, TRUE);
#ifndef _DEBUG
    TextIoClear();
    TextIoSetCursorPosition(0, 8);
#endif

    PrintT(
        "\n%s: Core %i, thread %X\n\nCritical system failure\n", 
        __FUNCTION__,
        KeGetCurrentProcessorId(), 
        KeGetCurrentThread());

    /* TODO */
    if (code == KMODE_EXCEPTION_NOT_HANDLED)
    {
        PrintT("KMODE_EXCEPTION_NOT_HANDLED");    
    }
    else if (code == PHASE1_INITIALIZATION_FAILED)
    {
        PrintT("PHASE1_INITIALIZATION_FAILED");
    }
    else if (code == HAL_INITIALIZATION_FAILED)
    {
        PrintT("HAL_INITIALIZATION_FAILED");
    }
    else if (code == IRQL_NOT_GREATER_OR_EQUAL)
    {
        PrintT("IRQL_NOT_GREATER_OR_EQUAL");
    }
    else if (code == IRQL_NOT_LESS_OR_EQUAL)
    {
        PrintT("IRQL_NOT_LESS_OR_EQUAL");
    }
    else
    {
        PrintT("BUGCHECK_CODE_%X", code);
    }

    PrintT("\n\n");
    PrintT("0x%X, 0x%X, 0x%X, 0x%X\n\n\n", param1, param2, param3, param4);
    PrintT("CR2: %H CR3: %H\n", __readcr2(), __readcr3());

    PrintT("DpcStack: %X\n\n", KeGetPcr()->Prcb->DpcStack);

    /* TODO: this is not updated in exception handlers currently */
    PrintT("Current stack:\n");
    PKTASK_STATE taskState = (PKTASK_STATE)KeGetCurrentThread()->KernelStackPointer;
    PrintT("RSP0   %X\n", taskState);
    PrintT("RIP    %X\n", taskState->Rip);
    PrintT("RSP    %X\n", taskState->Rsp);
    PrintT("SS     %X\n", taskState->Ss);
    PrintT("CS     %X\n", taskState->Cs);
    PrintT("RFLAGS %X\n", taskState->Rflags);

    PLIST_ENTRY entry, originalEntry;
    PKTHREAD thread;

    extern LIST_ENTRY ThreadListHead;

    originalEntry = &ThreadListHead;
    entry = originalEntry->Next;

    while (entry != originalEntry)
    {
        thread = CONTAINING_RECORD(entry, KTHREAD, ThreadListEntry);

        PrintT("THREAD %X\n", thread);
        PrintT("KernelStack: %X ", 
               thread->KernelStackPointer);
        PrintT("CurrentSize: %X ", 
               (ULONG_PTR)thread->OriginalKernelStackPointer - (ULONG_PTR)thread->KernelStackPointer);
        PrintT("Last stack switch: %x\n\n", 
               thread->SwitchStackPointer);

        entry = entry->Next;
    }

    HalDisableInterrupts();
    KeLowerIrql(0);

    KeStop();
}