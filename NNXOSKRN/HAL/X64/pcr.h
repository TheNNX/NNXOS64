#ifndef NNX_PCR_HEADER
#define NNX_PCR_HEADER

#include <nnxtype.h>
#include "../spinlock.h"
#include "../../../NNXOSLDR/HAL/GDT.h"
#include "../../../NNXOSLDR/HAL/IDT.h"
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
		PKGDTENTRY64 Gdt;
		PKTSS Tss;
		ULONG Reserved[2];
		struct _KPCR *SelfPcr;
		struct _KPRCB *Prcb;
		KIRQL   Irql;
		ULONG   Reserved1[5];
		PKIDTENTRY64 Idt;
		ULONG	Reserved4;
		ULONG	Reserved2[2];
		USHORT  MajorVersion;
		USHORT  MinorVersion;
		ULONG   Reserved3[36];
	}KPCR, *LPKPCR, *PKPCR;

	PKPCR KeGetPcr();
	PKPCR HalCreatePcr();
	VOID HalpSetupPcrForCurrentCpu(UINT64 id);

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
		ULONG64 RspBase;
		KSPIN_LOCK Lock;
	}KPRCB, *PKRCB, *LPKRCB;

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif
