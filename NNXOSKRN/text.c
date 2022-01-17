#include "text.h"

SIZE_T FindCharacterLast(const char* string, SIZE_T len, char character)
{
	SIZE_T current = -1;
	SIZE_T i;

	for (i = 0; i < len; i++)
	{
		if (string[i] == character)
		{
			current = i;
		}
	}

	return current;
}

SIZE_T FindCharacterFirst(const char* string, SIZE_T len, char character)
{
	SIZE_T i;

	for (i = 0; i < len; i++)
	{
		if (string[i] == character)
		{
			return i;
		}
	}

	return -1;
}

SIZE_T IntegerToAsciiBase(ULONG_PTR i, INT8 base, char *b, const char *digit)
{
	SIZE_T counter = 0;
	char* p = b;
	SIZE_T shifter = i;

	if (base == 0)
		return 0;

	if (base < 0 && ((INT64) i) < 0)
	{
		i = (-((INT64) i));
		if (b)
		{
			*b++ = '-';
		}
		else
		{
			counter++;
		}
	}

	if (base < 0)
		base = -base;


	if (b == 0)
	{
		do
		{
			i = i / base;
			counter++;
		}
		while (i);

		return counter;
	}

	do
	{
		++p;
		shifter = shifter / base;
		counter++;
	}
	while (shifter);
	*p = '\0';
	do
	{
		*--p = digit[i % base];
		i = i / base;
	}
	while (i);
	return counter;
}


/*
	Negative bases for signed numbers
*/
SIZE_T IntegerToAscii(ULONG_PTR i, INT8 base, char *b)
{
	return IntegerToAsciiBase(i, base, b, "0123456789abcdef");
}


/*
	Negative bases for signed numbers
*/
SIZE_T IntegerToAsciiCapital(ULONG_PTR i, INT8 base, char *b)
{
	return IntegerToAsciiBase(i, base, b, "0123456789ABCDEF");
}

char ToUppercase(char c)
{
	if (c >= 'a' && c <= 'z')
		return c + ('A' - 'a');
	return c;
}

char ToLowercase(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - ('A' - 'a');
	return c;
}