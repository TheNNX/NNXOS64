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
	struct GDT* Base;
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
	union 
	{
		UINT16 IopbOffset;
		UINT16 Size;
	};
	UINT16 reserved4;
	UINT64 reserved3;
	UINT64 Ist[8];
	UINT64 reserved2;
	UINT64 Rsp[3];
	UINT32 reserved1;
}KTSS;

typedef struct _KTSSENTRY64 
{
	UINT32 Reserved;
	UINT32 Base32To63;
	KGDTENTRY64 StandartGdtEntry;
}KTSSENTRY64, *PKTSSENTRY64, *LPKTSSENTRY64;

void LoadGDT(KGDTR64*);
void StoreGDT(KGDTR64*);

#ifdef __cplusplus
}
#endif

#pragma pack(pop)
#endif