#include <rtl.h>

#pragma function(memset)
#pragma function(memcpy)

void* memcpy(void *dst, void* src, SIZE_T size)
{
    RtlCopyMemory(dst, src, size);
    return dst;
}

void* memset(void* dst, UINT8 value, SIZE_T size)
{
    RtlFillMemory(dst, size, value);
    return dst;
}