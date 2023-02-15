#ifndef NNX_IDT_HEADER
#define NNX_IDT_HEADER

#ifdef __cplusplus
extern "C"
{
#endif

#include <nnxtype.h>
#include <HAL/interrupt.h>

#pragma pack(push)
#pragma pack(1)
	typedef struct _KIDTR64
	{
		UINT16 Size;
		struct _KIDTENTRY64* Base;
	}KIDTR64;

	typedef struct _KIDTENTRY64
	{
		UINT16 Offset0to15;
		UINT16 Selector;
		UINT8 Ist;
		UINT8 Type;
		UINT16 Offset16to31;
		UINT32 Offset32to63;
		UINT32 Zero;
	}KIDTENTRY64, *PKIDTENTRY64, *LPKIDTENTRY64;
#pragma pack(pop)

	void HalpLoadIdt(KIDTR64*);
	void HalpStoreIdt(KIDTR64*);

	void Exception0();
	void Exception1();
	void Exception2();
	void Exception3();
	void Exception4();
	void Exception5();
	void Exception6();
	void Exception7();
	void Exception8();
	void Exception10();
	void Exception11();
	void Exception12();
	void Exception13();
	void Exception14();
	void Exception16();
	void Exception17();
	void Exception18();
	void Exception19();
	void Exception20();
	void Exception30();
	void ExceptionReserved();
	void ExceptionHandler(UINT64 number, UINT64 errorCode, UINT64 errorCode2, UINT64 rip);
	void IrqHandler();

	PKIDTENTRY64 
		HalpAllocateAndInitializeIdt();
	
	KIDTENTRY64
	NTAPI 
	HalpSetIdtEntry(
		KIDTENTRY64* idt, 
		UINT64 entryNo, 
		PVOID handler, 
		BOOL userCallable, 
		BOOL trap);

	VOID
	NTAPI
	HalpInitInterruptHandlerStub(
		PKINTERRUPT pInterrupt,
		ULONG_PTR ProperHandler);
#ifdef __cplusplus
}
#endif

#endif