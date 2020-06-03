#ifndef NNX_GDT_HEADER
#define NNX_GDT_HEADER
#pragma pack(push)
#pragma pack(1)
#include "nnxint.h"

struct GDT;

typedef struct GDTR {
	UINT16 size;
	struct GDT* offset;
}GDTR;

typedef struct GDTEntry {
	UINT16 limit0to15;
	UINT16 base0to15;
	UINT8 base16to23;
	UINT8 accessByte;
	UINT8 limit16to19 : 4;
	UINT8 flags : 4;
	UINT8 base24to31;
}GDTEntry;

typedef struct GDT {
	GDTEntry entries[0];
}GDT;

typedef struct TSS {
	union {
		UINT16 IOPB_offset;
		UINT16 Size;
	};
	UINT16 reserved4;
	UINT64 reserved3;
	UINT64 IST[8];
	UINT64 reserved2;
	UINT64 RSP[3];
	UINT32 reserved1;
}TSS;

typedef struct TSSDescriptorEntry {
	UINT32 reserved;
	UINT32 base32to63;
	GDTEntry StandartGdtEntry;
}TSSDescriptorEntry;

void LoadGDT(GDTR*);
void StoreGDT(GDTR*);
#pragma pack(pop)
#endif