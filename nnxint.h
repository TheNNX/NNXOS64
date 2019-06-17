#pragma once
#ifndef _NNXINT
#define _NNXINT

typedef unsigned long long int UINT64;
typedef long long int INT64;

typedef unsigned int UINT32;
typedef int INT32;

typedef unsigned short int UINT16;
typedef short int INT16;

typedef unsigned char UINT8;
typedef signed char INT8;

typedef UINT64 QWORD;
typedef UINT32 DWORD;
typedef UINT16 WORD;
typedef UINT8 BYTE;

#define GetBit(num,n) (num >> n) & 1

#endif // !_NNXINT
