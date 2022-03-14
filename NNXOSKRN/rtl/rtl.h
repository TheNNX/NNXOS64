#ifndef NNX_RTL_HEADER
#define NNX_RTL_HEADER

#ifdef __cplusplus
extern "C"
{
#endif

#include <nnxtype.h>

    VOID RtlZeroMemory(PVOID Memory, SIZE_T MemorySize);

#include <rtl/rtlstring.h>

#ifdef __cplusplus
}
#endif

#endif