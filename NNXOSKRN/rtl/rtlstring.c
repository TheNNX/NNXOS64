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

NTSTATUS
NTAPI
RtlUnicodeStringCat(
    PUNICODE_STRING  DestinationString,
    PCUNICODE_STRING SourceString)
{
    SIZE_T i;

    if (DestinationString == NULL || SourceString == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (DestinationString->Buffer == NULL ||
        SourceString->Buffer == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (DestinationString->Length == DestinationString->MaxLength &&
        SourceString->Length > 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    for (i = 0; 
         i < SourceString->Length / 2 && 
         DestinationString->Length < DestinationString->MaxLength;
         i++)
    {
        DestinationString->Buffer[DestinationString->Length / 2] =
            SourceString->Buffer[i];

        DestinationString->Length += sizeof(WCHAR);
    }

    if (i < SourceString->Length / 2)
    {
        return STATUS_BUFFER_OVERFLOW;
    }

    return STATUS_SUCCESS;
}