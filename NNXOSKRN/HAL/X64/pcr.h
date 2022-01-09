#ifndef NNX_PCR_HEADER
#define NNX_PCR_HEADER

#include <nnxtype.h>
#include "../spinlock.h"
#include <HAL/GDT.h>
#include <HAL/IDT.h>
#include "../irql.h"

#ifdef __cplusplus
extern "C"
{
#endif

#pragma pack(push)
#pragma pack(1)
	/* TODO */
	typedef struct _KPCR
	{
		PKGDTENTRY64	Gdt;
		PKTSS			Tss;
		LONG_PTR		CyclesLeft;
		struct _KPCR	*SelfPcr;
		struct _KPRCB	*Prcb;
		KIRQL			Irql;
		ULONG			Reserved1[5];
		PKIDTENTRY64	Idt;
		ULONG			Reserved4[3];
		USHORT			MajorVersion;
		USHORT			MinorVersion;
		ULONG			Reserved3[36];
	}KPCR, *LPKPCR, *PKPCR;

	PKPCR KeGetPcr();
	PKPCR HalCreatePcr(PKGDTENTRY64 gdt, PKIDTENTRY64 idt, UCHAR CoreNumber);
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
		ULONG_PTR CpuCyclesRemaining;
		KSPIN_LOCK Lock;
	}KPRCB, *PKPRCB, *LPKRCB;

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif
