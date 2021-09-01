#ifndef NNX_TEXT_HEADER
#define NNX_TEXT_HEADER

#include "nnxint.h"
#include "nnxosdbg.h"

#ifdef __cplusplus
extern "C"{
#endif

UINT64 FindCharacterLast(char* string, UINT64 len, char character);

/*
len can be 0xFFFFFFFF/-1 if length is undefined, for ex. you can use FindCharacterFirst(string, -1, 0) to measure the length itself
*/
UINT64 FindCharacterFirst(char* string, UINT64 len, char character);

UINT64 IntegerToAscii(UINT64 i, INT8 base, char* b);
UINT64 IntegerToAsciiCapital(UINT64 i, INT8 base, char* b);

char ToUppercase(char);
char ToLowercase(char);

#ifdef __cplusplus
}
#endif

#endif