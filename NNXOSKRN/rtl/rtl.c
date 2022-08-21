#include "rtl.h"

VOID RtlFillMemory(PVOID Memory, SIZE_T Size, INT Value)
{
    SIZE_T i;
    PUCHAR pMem = (PUCHAR)Memory;

    for (i = 0; i < Size; i++)
    {
        pMem[i] = (BYTE)Value;
    }
}

VOID RtlZeroMemory(PVOID Memory, SIZE_T Size)
{
    RtlFillMemory(Memory, Size, 0);
}