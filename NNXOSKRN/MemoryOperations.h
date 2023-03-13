#ifndef NNX_MEMOP_HEADER
#define NNX_MEMOP_HEADER

#include <nnxtype.h>
#ifdef  __cplusplus
extern "C"
{
#endif

    void* MemSet(void* dst, UINT8 value, SIZE_T size);
    void* MemCopy(void* dst, void* src, SIZE_T size);

#ifdef __cplusplus
}
#endif 
#endif