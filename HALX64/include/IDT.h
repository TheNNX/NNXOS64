#ifndef NNX_IDT_HEADER
#define NNX_IDT_HEADER

#ifdef __cplusplus
extern "C"
{
#endif

#include <nnxtype.h>
#include <interrupt.h>

#pragma pack(push)
#pragma pack(1)
    typedef struct _KIDTR64
    {
        UINT16 Size;
        struct _KIDTENTRY64* Base;
    }KIDTR64, *PKIDTR64;

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

    NTHALAPI
    VOID
    NTAPI
    ExceptionHandler(
        ULONG_PTR n,
        ULONG_PTR errcode,
        ULONG_PTR errcode2,
        ULONG_PTR rip);

    NTHALAPI
    VOID
    NTAPI
    HalpInitializeIdt(
        PKIDTENTRY64 Idt,
        PKIDTR64 Idtr);
    
    NTHALAPI
    KIDTENTRY64
    NTAPI 
    HalBindInterrupt(
        KIDTENTRY64* idt, 
        UINT64 entryNo, 
        PVOID handler, 
        BOOL userCallable, 
        BOOL trap);

    NTHALAPI
    VOID
    NTAPI
    HalpInitInterruptHandlerStub(
        PKINTERRUPT pInterrupt,
        ULONG_PTR ProperHandler);

    NTHALAPI
    VOID
    NTAPI
    HalpInitLegacyInterruptHandlerStub(
        PKINTERRUPT pInterrupt,
        ULONG_PTR ProperHandler);
#ifdef __cplusplus
}
#endif

#endif