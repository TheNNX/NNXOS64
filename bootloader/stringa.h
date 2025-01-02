#ifndef NNX_STRINGA_H
#define NNX_STRINGA_H

#include <efi.h>
#include <efilib.h>

#ifdef __cplusplus
extern "C" {
#endif

inline
INTN
StrCmpA(
    IN const CHAR8* Str1,
    IN const CHAR8* Str2)
{
    while (*Str1)
    {
        if (*Str1++ != *Str2++)
        {
            break;
        }
    }

    return *Str1 - *Str2;
}

inline
size_t
StrLenA(
    IN const CHAR8* Str)
{
    return strlena(Str);
}

inline
CHAR8*
StrCpyA(
    OUT CHAR8* Dest,
    IN const CHAR8* Src)
{
    CopyMem(Dest, Src, StrLenA(Src) + 1);
    return Dest;
}

inline
void       
StrCatA(
    OUT CHAR8* Dest,
    IN const CHAR8* Src)
{
    StrCpyA(Dest + StrLenA(Dest), Src);
}

inline
void
StrWToStrA(
    OUT CHAR8* Dest,
    IN const CHAR16* Src)
{
    size_t len = StrLen(Src);
    size_t i;
    
    for (i = 0; i < len; i++)
    {
        Dest[i] = (CHAR8)(Src[i]);
        Dest[i + 1] = '\0';
    }
}

#ifdef __cplusplus
}
#endif

#endif