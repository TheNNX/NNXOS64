#pragma once
#include "nnxint.h"
#ifdef  __cplusplus
extern "C" {
#endif

	void memset(void* dst, UINT64 value, UINT64 size);
	void memcpy(void* dst, void* src, UINT64 size);

#ifdef __cplusplus
}
#endif 
