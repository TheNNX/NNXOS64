#include "text.h"

UINT64 FindCharacterLast(char* string, UINT64 len, char character) {
	NNXAssertAndStop(len != -1, "Cannot find the last character of string with unknown length.");

	UINT64 current = -1;

	for (UINT64 i = 0; i < len; i++) {
		if (string[i] == character) {
			current = i;
		}
	}

	return current;
}

UINT64 FindCharacterFirst(char* string, UINT64 len, char character) {
	for (UINT64 i = 0; i < len; i++) {
		if (string[i] == character) {
			return i;
		}
	}

	return -1;
}