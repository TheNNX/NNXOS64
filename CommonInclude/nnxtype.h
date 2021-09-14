#ifndef NNX_INT_HEADER
#define NNX_INT_HEADER

typedef unsigned __int32	ULONG, *LPULONG, *PULONG;
typedef __int32				LONG, *LPLONG, *PLONG;

typedef unsigned __int16	USHORT, *LPUSHORT, *PUSHORT;
typedef __int16				SHORT, *LPSHORT, *PSHORT;

typedef unsigned __int8		UBYTE, *LPUBYTE, *PUBYTE;
typedef __int8				BYTE, *LPBYTE, *PBYTE;

typedef unsigned __int64	ULONGLONG, *PULONGLONG, *LPULONGLONG;
typedef __int64				LONGLONG, *PLONGLONG, *LPLONGLONG;

typedef unsigned __int64	UINT64;
typedef __int64				INT64;

typedef unsigned __int32	UINT32;
typedef __int32				INT32;

typedef unsigned __int16	UINT16;
typedef __int16				INT16;

typedef unsigned __int8		UINT8;
typedef __int8				INT8;

typedef UINT64	QWORD;
typedef UINT32	DWORD;
typedef UINT16	WORD;
typedef UINT8	UCHAR;
typedef INT8	CHAR;

#ifndef WCHAR_MAX
typedef UINT16	WCHAR;
#endif

#ifndef TRUE
#define TRUE 1
#endif 

#ifndef FALSE
#define FALSE 0
#endif

#ifndef __cplusplus
	typedef UINT8 bool;
	#define true TRUE
	#define false FALSE
#endif

typedef bool BOOL;

#ifndef VOID
typedef void VOID;
#endif
typedef VOID *PVOID, *LPVOID;

#ifndef NULL
#define NULL ((LPVOID*)0)
#endif

#ifdef _M_X86
#define NTAPI __cdecl
#define FASTCALL __fastcall
#endif

#ifdef _M_AMD64
#define NTAPI __cdecl
#define FASTCALL __fastcall
#endif

inline bool GetBit(unsigned int num, unsigned int n) {
	return ((num >> n) & 1);
}
#endif
