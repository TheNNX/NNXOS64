#ifndef NNX_GDT_HEADER
#define NNX_GDT_HEADER
#pragma pack(push)
#pragma pack(1)
#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif

struct GDT;

typedef struct GDTR {
	UINT16 Size;
	struct GDT* Base;
}GDTR;

typedef struct GDTEntry {
	UINT16 Limit0To15;
	UINT16 Base0To15;
	UINT8 Base16To23;
	UINT8 AccessByte;
	UINT8 Limit16To19 : 4;
	UINT8 Flags : 4;
	UINT8 Base24To31;
}GDTEntry;

typedef struct GDT {
	GDTEntry Entries[0];
}GDT;

typedef struct TSS {
	union {
		UINT16 IopbOffset;
		UINT16 Size;
	};
	UINT16 reserved4;
	UINT64 reserved3;
	UINT64 Ist[8];
	UINT64 reserved2;
	UINT64 Rsp[3];
	UINT32 reserved1;
}TSS;

typedef struct TSSDescriptorEntry {
	UINT32 Reserved;
	UINT32 Base32To63;
	GDTEntry StandartGdtEntry;
}TSSDescriptorEntry;

void LoadGDT(GDTR*);
void StoreGDT(GDTR*);

#ifdef __cplusplus
}
#endif

#pragma pack(pop)
#endif