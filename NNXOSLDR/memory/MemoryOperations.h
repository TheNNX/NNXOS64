#ifndef NNX_MEMOP_HEADER
#define NNX_MEMOP_HEADER

#include <nnxint.h>
#ifdef  __cplusplus
extern "C" {
#endif

	void MemSet(void* dst, UINT8 value, UINT64 size);
	void MemCopy(void* dst, void* src, UINT64 size);

#ifdef __cplusplus
}
#endif 
#endif