#ifndef NNX_IDT_HEADER
#define NNX_IDT_HEADER
#pragma pack(push)
#pragma pack(1)

#ifdef __cplusplus
extern "C"
{
#endif

#include <nnxtype.h>

	typedef struct _KIDTR64
	{
		UINT16 Size;
		struct IDT* Base;
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

	void LoadIDT(KIDTR64*);
	void StoreIDT(KIDTR64*);
	void EnableInterrupts();
	void DisableInterrupts();
	void ForceInterrupt(UINT64);
	void Ack(UINT64);

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
	void ExceptionHandler(UINT64 number, UINT64 errorCode, UINT64 rip);

	void IRQ0();
	void IRQ1();
	void IRQ2();
	void IRQ3();
	void IRQ4();
	void IRQ5();
	void IRQ6();
	void IRQ7();
	void IRQ8();
	void IRQ9();
	void IRQ10();
	void IRQ11();
	void IRQ12();
	void IRQ13();
	void IRQ14();
	void IrqHandler(UINT64);

#ifdef __cplusplus
}
#endif

#pragma pack(pop)
#endif