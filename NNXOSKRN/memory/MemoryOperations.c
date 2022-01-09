#include "MemoryOperations.h"

void MemSet(void* dest, UINT8 value, UINT64 c)
{
    INT b;
    PBYTE destAsBytePtr = (PBYTE)dest;

    for (b = 0; b < c; b++)
    {
        *destAsBytePtr++ = value;
    }
}

void MemCopy(void* dst, void *src, UINT64 size)
{
    for (int b = 0; b < size; b++)
    {
        ((UINT8*) dst)[b] = ((UINT8*) src)[b];
    }
}