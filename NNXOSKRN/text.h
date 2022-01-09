#ifndef NNX_TEXT_HEADER
#define NNX_TEXT_HEADER

#include <nnxtype.h>
#include "nnxosdbg.h"

#ifdef __cplusplus
extern "C" {
#endif

	SIZE_T FindCharacterLast(const char* string, SIZE_T len, char character);

	/*
	len can be -1 if length is undefined, for ex. you can use FindCharacterFirst(string, -1, 0) to measure the length itself
	*/
	SIZE_T FindCharacterFirst(const char* string, SIZE_T len, char character);

	SIZE_T IntegerToAscii(ULONG_PTR i, INT8 base, char* b);
	SIZE_T IntegerToAsciiCapital(ULONG_PTR i, INT8 base, char* b);

	char ToUppercase(char);
	char ToLowercase(char);

#ifdef __cplusplus
}
#endif

#endif