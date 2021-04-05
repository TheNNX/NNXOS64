#ifndef NNX_TEXT_H
#define NNX_TEXT_H

#include "nnxint.h"
#include "nnxosdbg.h"

UINT64 FindCharacterLast(char* string, UINT64 len, char character);

/*
len can be 0xFFFFFFFF/-1 if length is undefined, for ex. you can use FindCharacterFirst(string, -1, 0) to measure the length itself
*/
UINT64 FindCharacterFirst(char* string, UINT64 len, char character);

#endif