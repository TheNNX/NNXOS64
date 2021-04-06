#ifndef NNX_HDD_HEADER
#define NNX_HDD_HEADER
#include "nnxint.h"
#pragma pack(push, 1)

void DiskCheck();

typedef struct CHS {
	union {
		UINT32 chs;
		struct
		{
			UINT32 sector : 6;
			UINT32 head : 4;
			UINT32 cylinder : 14;
		};
	};
}CHS;

typedef struct HSC {
	BYTE bytes[3];
}HSC;

#pragma pack(pop)

#endif