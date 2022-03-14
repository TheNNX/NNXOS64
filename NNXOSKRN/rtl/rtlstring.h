#ifndef NNX_RTLSTR_H
#define NNX_RTLSTR_H

#include <nnxtype.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct _UNICODE_STRING
    {
        USHORT Length;
        USHORT MaxLength;
        PWSTR Buffer;

    }UNICODE_STRING, *PUNICODE_STRING;

    typedef const UNICODE_STRING CUNICODE_STRING, *PCUNICODE_STRING;

    typedef struct _STRING
    {
        USHORT Length;
        USHORT MaxLength;
        PCHAR Buffer;
    }STRING, *PSTRING;

    typedef const STRING CSTRING, *PCSTRING;

    BOOL NTAPI RtlEqualUnicodeString(PCUNICODE_STRING, PCUNICODE_STRING, BOOL IgnoreCase);
    LONG NTAPI RtlCompareUnicodeString(PCUNICODE_STRING, PCUNICODE_STRING, BOOL IgnoreCase);
    BOOL NTAPI RtlEqualString(PCSTRING, PCSTRING, BOOL IgnoreCase);
    LONG NTAPI RtlCompareString(PCSTRING, PCSTRING, BOOL IgnoreCase);

#define RTL_CONSTANT_STRING(s) {sizeof(s)-sizeof(*s), sizeof(s), s}

#ifdef __cplusplus
}
#endif
#endif