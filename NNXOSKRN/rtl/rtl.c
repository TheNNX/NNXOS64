#include "rtl.h"

VOID 
NTAPI
RtlFillMemory(
    PVOID Memory, 
    SIZE_T Size, 
    INT Value)
{
    SIZE_T i;
    PUCHAR pMem = (PUCHAR)Memory;

    for (i = 0; i < Size; i++)
    {
        pMem[i] = (BYTE)Value;
    }
}

VOID 
NTAPI
RtlZeroMemory(
    PVOID Memory, 
    SIZE_T Size)
{
    RtlFillMemory(Memory, Size, 0);
}

VOID
NTAPI
RtlCopyMemory(
    PVOID dst, 
    PVOID src, 
    SIZE_T size)
{
    for (int b = 0; b < size; b++)
    {
        ((UINT8*)dst)[b] = ((UINT8*)src)[b];
    }
}