#pragma once
#pragma pack(push)
#pragma pack(1)

#include "nnxint.h"

struct IDT;
typedef struct IDTR {
	UINT16 size;
	struct IDT* offset;
}IDTR;

typedef struct IDTEntry {
	UINT16 offset0to15;
	UINT16 selector;
	UINT8 ist;
	UINT8 type;
	UINT16 offset16to31;
	UINT32 offset32to63;
	UINT32 zero;
}IDTEntry;

typedef struct IDT {
	IDTEntry entries[0];
}IDT;

void LoadIDT(IDTR*);
void StoreIDT(IDTR*);
void EnableInterrupts();
void DisableInterrupts();
void ForceInterrupt(UINT8);

#pragma pack(pop)