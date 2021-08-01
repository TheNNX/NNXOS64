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

UINT64 IntegerToASCIIBase(UINT64 i, INT8 base, char *b, const char *digit) {
	UINT64 counter = 0;
	char* p = b;
	UINT64 shifter = i;

	if (base == 0)
		return 0;

	if (base < 0 && ((INT64)i) < 0) {
		i = (-((INT64)i));
		if (b) {
			*b++ = '-';
		}
		else {
			counter++;
		}
	}

	if(base < 0)
		base = -base;
	

	if (b == 0) {
		do {
			i = i / base;
			counter++;
		} while (i);

		return counter;
	}

	do {
		++p;
		shifter = shifter / base;
		counter++;
	} while (shifter);
	*p = '\0';
	do {
		*--p = digit[i % base];
		i = i / base;
	} while (i);
	return counter;
}


/*
	Negative bases for signed numbers
*/
UINT64 IntegerToASCII(UINT64 i, INT8 base, char *b)
{
	return IntegerToASCIIBase(i, base, b, "0123456789abcdef");
}


/*
	Negative bases for signed numbers
*/
UINT64 IntegerToASCIICapital(UINT64 i, INT8 base, char *b)
{
	return IntegerToASCIIBase(i, base, b, "0123456789ABCDEF");
}

char ToUppercase(char c) {
	if (c >= 'a' && c <= 'z')
		return c + ('A' - 'a');
	return c;
}

char ToLowercase(char c) {
	if (c >= 'A' && c <= 'Z')
		return c - ('A' - 'a');
	return c;
}