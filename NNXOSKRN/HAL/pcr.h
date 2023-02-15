#ifndef NNX_PCR_HEADER
#define NNX_PCR_HEADER

#include <nnxtype.h>
#include <HAL/spinlock.h>
#include <HAL/irql.h>
#include <ntlist.h>

#ifdef __cplusplus
extern "C"
{
#endif

#pragma pack(push)
#pragma pack(1)

#ifdef _M_AMD64

#include <HAL/X64/GDT.h>
#include <HAL/X64/IDT.h>

	unsigned __int64 __readgsqword(unsigned long);
	unsigned long	 __readgsdword(unsigned long);
	unsigned short	 __readgsword(unsigned long);
	unsigned char    __readgsbyte(unsigned long);
	void __writegsbyte(unsigned long, unsigned char);
	void __writegsword(unsigned long, unsigned short);
	void __writegsdword(unsigned long, unsigned long);
	void __writegsqword(unsigned long, unsigned __int64);

#define FIELD_OFFSET(t,field) ((ULONG_PTR)&(((t*)0)->field))

	typedef struct _KPCR
	{
		PKGDTENTRY64	Gdt;
		PKTSS			Tss;
		LONG_PTR		CyclesLeft;
		struct _KPCR	*SelfPcr;
		struct _KPRCB	*Prcb;
		KIRQL			Irql;

		/* These have to be accessed with interrupts disabled.
		 * Once interrupts are reenabled, the value of these should
		 * be assumed to be invalid. They're used by the system call
		 * handler to store thread pointers in order to get the thread
		 * kernel stack and access other per thread variables. */
		ULONG_PTR		TempHandlerVals[2];

		LONG			Reserved0;
		PKIDTENTRY64	Idt;
		ULONG			Reserved4[3];
		USHORT			MajorVersion;
		USHORT			MinorVersion;
		LIST_ENTRY		InterruptListHead;
		PKINTERRUPT		ClockInterrupt;
	}KPCR, *LPKPCR, *PKPCR;

	PKPCR HalCreatePcr(PKGDTENTRY64 gdt, PKIDTENTRY64 idt, UCHAR CoreNumber);
	VOID HalpSetDummyPcr();
	VOID HalpInitDummyPcr();
	VOID HalSetPcr(PKPCR);
#else
#error "Architecture unsupported"
#endif

	PKPCR KeGetPcr();
	VOID HalpSetupPcrForCurrentCpu(UCHAR id);

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
		ULONG_PTR Reserved;
		KSPIN_LOCK Lock;
	}KPRCB, *PKPRCB, *LPKRCB;

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif
