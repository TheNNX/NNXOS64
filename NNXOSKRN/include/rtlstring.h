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

    NTSYSAPI
    BOOLEAN
    NTAPI 
    RtlEqualUnicodeString(
        PCUNICODE_STRING, 
        PCUNICODE_STRING, 
        BOOLEAN IgnoreCase);
    
    NTSYSAPI
    LONG 
    NTAPI 
    RtlCompareUnicodeString(
        PCUNICODE_STRING, 
        PCUNICODE_STRING, 
        BOOLEAN IgnoreCase);
    
    NTSYSAPI
    BOOLEAN
    NTAPI 
    RtlEqualString(
        PCSTRING, 
        PCSTRING, 
        BOOLEAN IgnoreCase);
    
    NTSYSAPI
    LONG 
    NTAPI 
    RtlCompareString(
        PCSTRING, 
        PCSTRING, 
        BOOLEAN IgnoreCase);

#define RTL_CONSTANT_STRING(s) {sizeof(s)-sizeof(*s), sizeof(s), s}

#ifdef NNX_KERNEL
    inline static void DebugWPrint(PUNICODE_STRING unicodeStr)
    {
        VOID PrintTA(const char*, ...);

        USHORT i;

        for (i = 0; i < unicodeStr->Length / sizeof(*unicodeStr->Buffer); i++)
            PrintTA("%c", (UCHAR)(unicodeStr->Buffer[i]));
    }
#endif

#ifdef __cplusplus
}
#endif
#endif