#include "MemoryOperations.h"

void* MemSet(void* dest, UINT8 value, SIZE_T c)
{
    INT b;
    PBYTE destAsBytePtr = (PBYTE)dest;

    for (b = 0; b < c; b++)
    {
        *destAsBytePtr++ = value;
    }

    return dest;
}

void* MemCopy(void* dst, void *src, SIZE_T size)
{
    for (int b = 0; b < size; b++)
    {
        ((UINT8*) dst)[b] = ((UINT8*) src)[b];
    }

    return dst;
}

#ifndef _DEBUG
#pragma function(memset)
#pragma function(memcpy)

void* memcpy(void *dst, void* src, SIZE_T size)
{
    return MemCopy(dst, src, size);
}

void* memset(void* dst, UINT8 value, SIZE_T size)
{
    return MemSet(dst, value, size);
}
#endif