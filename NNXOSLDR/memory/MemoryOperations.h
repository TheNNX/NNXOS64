#ifndef NNX_MEMOP_HEADER
#define NNX_MEMOP_HEADER

#include "nnxint.h"
#ifdef  __cplusplus
extern "C" {
#endif

	void memset(void* dst, UINT8 value, UINT64 size);
	void memcpy(void* dst, void* src, UINT64 size);

#ifdef __cplusplus
}
#endif 
#endif