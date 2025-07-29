#ifndef NNX_PCR_HEADER
#define NNX_PCR_HEADER

#include <nnxtype.h>
#include <spinlock.h>
#include <irql.h>
#include <ntlist.h>
#include <dpc.h>

#ifdef __cplusplus
extern "C"
{
#endif

#pragma pack(push)
#pragma pack(1)

#ifdef _M_AMD64

#include <HALX64/include/GDT.h>
#include <HALX64/include/IDT.h>
#include <intrin.h>

#define FIELD_OFFSET(t,field) ((ULONG_PTR)&(((t*)0)->field))

    typedef struct _KPCR
    {
        PKGDTENTRY64    Gdt;
        PKTSS           Tss;
        LONG_PTR        CyclesLeft;
        struct _KPCR    *SelfPcr;
        struct _KPRCB   *Prcb;
        KIRQL           Irql;

        /* These have to be accessed with interrupts disabled.
         * Once interrupts are reenabled, the value of these should
         * be assumed to be invalid. They're used by the system call
         * handler to store thread pointers in order to get the thread
         * kernel stack and access other per thread variables. */
        ULONG_PTR       TempHandlerVals[2];

        LONG            Reserved0;
        PKIDTENTRY64    Idt;
        ULONG           Reserved4[3];
        USHORT          MajorVersion;
        USHORT          MinorVersion;
        LIST_ENTRY      InterruptListHead;
        PKINTERRUPT     ClockInterrupt;
    }KPCR, *LPKPCR, *PKPCR;

    PKPCR HalCreatePcr(PKGDTENTRY64 gdt, PKIDTENTRY64 idt, UCHAR CoreNumber);
    VOID HalpSetDummyPcr();
    VOID HalpInitDummyPcr();
    VOID HalSetPcr(PKPCR);

    typedef struct _KARCH_CORE_DATA
    {
        KGDTENTRY64 GdtEntires[16];
        KIDTENTRY64 IdtEntries[256];
        KIDTR64     Idtr;
        KGDTR64     Gdtr;
        KTSS        Tss;
    } KARCH_CORE_DATA, * PKARCH_CORE_DATA;
#else
#error "Architecture unsupported"
#endif

    typedef struct _KPRCB
    {
        ULONG MxCsr;
        USHORT LegacyNumber;
        UCHAR InterruptRequest;
        UCHAR IdleHalt;
        struct _KTHREAD* CurrentThread;
        struct _KTHREAD* NextThread;
        struct _KTHREAD* IdleThread;
        UCHAR NestingLevel;
        UCHAR Pad0[3];
        ULONG Number;
        struct _KTHREAD* DummyThread;
        KSPIN_LOCK Lock;
        KDPC_DATA DpcData;
        PVOID DpcStack;
        BOOLEAN DpcInProgress;
        BOOLEAN DpcEnding;
        KDPC SchedulerNotifyDpc;
    } KPRCB, *PKPRCB, *LPKRCB;

    PKPCR KeGetPcr();
    PKPRCB HalCreatePrcb(UCHAR CoreNumber);
    VOID HalpSetupPcrForCurrentCpu(UCHAR id);

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif
