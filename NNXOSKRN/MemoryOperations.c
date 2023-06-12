#include <rtl.h>

#ifndef _DEBUG
#pragma function(memset)
#pragma function(memcpy)

void* memcpy(void *dst, void* src, SIZE_T size)
{
    return RtlCopyMemory(dst, src, size);
}

void* memset(void* dst, UINT8 value, SIZE_T size)
{
    return RtlFillMemory(dst, size, value);
}
#endif