#ifndef NNX_INT_HEADER
#define NNX_INT_HEADER

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

#define TRUE 1
#define FALSE 0

#ifndef __cplusplus
	typedef UINT8 bool;
	#define true TRUE
	#define false FALSE
#endif

typedef bool BOOL;

#ifndef VOID
typedef void VOID;
#endif

typedef void* PVOID;

inline bool GetBit(unsigned int num, unsigned int n) {
	return ((num >> n) & 1);
}
#endif
