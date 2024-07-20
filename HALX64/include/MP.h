#ifndef NNX_MP_X64_HEADER
#define NNX_MP_X64_HEADER

#include <cpu.h>
#include <nnxtype.h>
#include <HALX64/include/GDT.h>
#include <HALX64/include/IDT.h>

#ifdef __cplusplus
extern "C" {
#endif
    VOID MpInitialize();
    VOID ApProcessorInit(UINT8 coreNumber);

#pragma pack(push, 1)
    typedef struct _AP_DATA
    {
        UINT16 ApSpinlock;
        UINT8 Padding[62];
        UINT8 ApNumberOfProcessors;
        UINT64 ApCR3;
        PVOID* ApStackPointerArray;
        VOID(*ApProcessorInit)(UINT8 coreNumber);
        KGDTR64 ApGdtr;
        _KIDTR64 ApIdtr;
        ULONG_PTR Output;
        UCHAR* ApLapicIds;
    }AP_DATA;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#define AP_INITIAL_STACK_SIZE 512

#endif