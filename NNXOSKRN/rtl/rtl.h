#ifndef NNX_RTL_HEADER
#define NNX_RTL_HEADER

#ifdef __cplusplus
extern "C"
{
#endif

#include <nnxtype.h>

    NTSYSAPI
    VOID 
    NTAPI
    RtlZeroMemory(
        PVOID Memory, 
        SIZE_T MemorySize);
    
    NTSYSAPI
    VOID 
    NTAPI
    RtlFillMemory(
        PVOID Memory, 
        SIZE_T Length, 
        INT Fill);

#include <rtl/rtlstring.h>

#ifdef __cplusplus
}
#endif

#endif