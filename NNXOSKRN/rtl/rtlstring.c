#include "rtlstring.h"

BOOLEAN
NTAPI 
RtlEqualUnicodeString(
    PCUNICODE_STRING A, 
    PCUNICODE_STRING B, 
    BOOLEAN IgnoreCase)
{
    return RtlCompareUnicodeString(A, B, IgnoreCase) == 0;
}

LONG 
NTAPI 
RtlCompareUnicodeString(
    PCUNICODE_STRING A, 
    PCUNICODE_STRING B, 
    BOOLEAN IgnoreCase)
{
    PWSTR a, b;
    USHORT i;

    if (A->Length > B->Length)
        return 1;
    if (A->Length < B->Length)
        return -1;

    a = A->Buffer;
    b = B->Buffer;

    for (i = 0; i < A->Length / sizeof(*A->Buffer); i++)
    {
        if (a[i] > b[i])
            return 1;
        if (a[i] < b[i])
            return -1;
    }

    return 0;
}

BOOLEAN
NTAPI 
RtlEqualString(
    PCSTRING A, 
    PCSTRING B, 
    BOOLEAN IgnoreCase)
{
    return RtlCompareString(A, B, IgnoreCase) == 0;
}

LONG
NTAPI 
RtlCompareString(
    PCSTRING A, 
    PCSTRING B, 
    BOOLEAN IgnoreCase)
{
    PCHAR a, b;
    USHORT i;

    if (A->Length > B->Length)
        return 1;
    if (A->Length < B->Length)
        return -1;

    a = A->Buffer;
    b = B->Buffer;

    for (i = 0; i < A->Length / sizeof(*A->Buffer); i++)
    {
        if (a[i] > b[i])
            return 1;
        if (a[i] < b[i])
            return -1;
    }

    return 0;
}
