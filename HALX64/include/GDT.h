#ifndef NNX_GDT_HEADER
#define NNX_GDT_HEADER
#pragma pack(push)
#pragma pack(1)
#include <nnxtype.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct _KGDTR64
    {
        UINT16 Size;
        struct _KGDTENTRY64* Base;
    }KGDTR64, *LPGDTR64, *PGDTR64;

    typedef struct _KGDTENTRY64
    {
        UINT16 Limit0To15;
        UINT16 Base0To15;
        UINT8 Base16To23;
        UINT8 AccessByte;
        UINT8 Limit16To19 : 4;
        UINT8 Flags : 4;
        UINT8 Base24To31;
    }KGDTENTRY64, *PKGDTENTRY64, *LPKGDTENTRY64;

    typedef struct _KTSS
    {
        UINT32 Reserved0;
        union
        {
            struct
            {
                UINT64 Rsp0;
                UINT64 Rsp1;
                UINT64 Rsp2;
            };
            UINT64 Rsp[3];
        };
        UINT64 Reserved1;
        union
        {
            struct
            {
                UINT64 Ist1;
                UINT64 Ist2;
                UINT64 Ist3;
                UINT64 Ist4;
                UINT64 Ist5;
                UINT64 Ist6;
                UINT64 Ist7;
            };
            UINT64 Ist[7];
        };
        UINT16 Reserved[5];
        UINT16 IopbBase;
    }KTSS, *PKTSS;

    typedef KGDTENTRY64 KTSSENTRY64, *PKTSSENTRY64, *LPKTSSENTRY64;

#ifdef NNX_HAL
    VOID
    NTAPI
    HalpLoadGdt(
        KGDTR64*);

    VOID
    NTAPI
    HalpLoadTss(
        UINT64 gdtOffset);

    ULONG
    NTAPI
    HalpGetGdtBase(
        KGDTENTRY64 entry);
#endif

#if defined(NNX_KERNEL) | defined(NNX_HAL)
    
    NTHALAPI
    VOID
    NTAPI
    HalpInitializeGdt(
        PKGDTENTRY64 Gdt,
        KGDTR64* Gdtr,
        PKTSS Tss);

    NTHALAPI
    PKTSS 
    NTAPI
    HalpGetTssBase(
        KGDTENTRY64 tssEntryLow, 
        KGDTENTRY64 tssEntryHigh);
    
    NTHALAPI
    USHORT 
    NTAPI
    HalpGdtFindEntry(
        LPKGDTENTRY64 gdt,
        USHORT numberOfEntries,
        BOOL code,
        BOOL user);

#endif

#ifdef __cplusplus
}
#endif

#pragma pack(pop)
#endif