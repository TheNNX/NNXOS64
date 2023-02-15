#pragma once
#include <nnxtype.h>
#include <HAL/irql.h>
#include <HAL/spinlock.h>
#include <ntlist.h>

#ifdef __cplusplus
extern "C" 
{
#endif
    typedef BOOL(NTAPI*KSERVICE_ROUTINE)
        (struct _KINTERRUPT* Interrupt, PVOID ServiceCtx);
    typedef KSERVICE_ROUTINE *PKSERVICE_ROUTINE;
    typedef ULONG_PTR(NTAPI*KFULLCTX_ROUTINE)
        (struct _KINTERRUPT* Interrupt, struct _KTASK_STATE*);

    typedef struct _KINTERRUPT
    {
        KSPIN_LOCK          Lock;
        KIRQL               InterruptIrql;
        BYTE                Handler[32];
        UCHAR               Vector;
        BOOLEAN             Connected;
        ULONG               CpuNumber;
        LIST_ENTRY          CpuListEntry;
        PVOID               ServiceCtx;
        KSERVICE_ROUTINE    pfnServiceRoutine;
        KFULLCTX_ROUTINE    pfnFullCtxRoutine;
        BOOLEAN             Trap;
        BOOLEAN             (*pfnGetRequiresFullCtx)
                            (struct _KINTERRUPT* Interrupt);
        BOOLEAN             SendEOI;
    } KINTERRUPT, *PKINTERRUPT;

    BOOLEAN  NTAPI KeConnectInterrupt(PKINTERRUPT Interrupt);
    BOOLEAN  NTAPI KeDisconnectInterrupt(PKINTERRUPT Interrupt);
    NTSTATUS NTAPI KiInitializeInterrupts(VOID);
    NTSTATUS NTAPI KiCreateInterrupt(PKINTERRUPT* ppInterrupt);
    VOID EnableInterrupts();
    VOID DisableInterrupts();
#ifdef __cplusplus
}
#endif