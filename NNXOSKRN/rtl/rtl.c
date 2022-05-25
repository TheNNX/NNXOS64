#include "rtl.h"

VOID RtlZeroMemory(PVOID Memory, SIZE_T Size)
{
    SIZE_T i;
    PUCHAR pMem = (PUCHAR)Memory;

    for (i = 0; i < Size; i++)
    {
        pMem[i] = 0;
    }
}