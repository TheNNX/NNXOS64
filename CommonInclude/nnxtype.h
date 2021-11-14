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


typedef UINT64	ULONG64, *PULONG64, *LPULONG64;
typedef UINT64	QWORD, *PQWORD, *LPQWORD;
typedef UINT32	DWORD, *PDWORD, *LPDWORD;
typedef UINT16	WORD, *PWORD, *LPWORD;
typedef UINT8	UCHAR, *PUCHAR, *LPUCHAR;
typedef INT8	CHAR, *PCHAR, *LPCHAR;

typedef LONG NTSTATUS;

#define STATUS_SUCCESS		0x00000000UL
#define STATUS_NO_MEMORY	0xC0000017UL

#ifndef _M_AMD64
typedef ULONG ULONG_PTR;
#else
typedef ULONGLONG ULONG_PTR;
#endif

typedef ULONG_PTR SIZE_T;

#ifndef WCHAR_MAX
typedef UINT16	WCHAR;
#define WCHAR_MAX 65535U
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
typedef BOOL BOOLEAN;

#ifndef VOID
typedef void VOID;
#endif
typedef VOID *PVOID, *LPVOID;

#ifndef NULL
#ifdef __cplusplus
#define NULL __nullptr
#else
#define NULL ((LPVOID*)0)
#endif
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

#define UINT64_MAX	0xFFFFFFFFFFFFFFFFULL
#define UINT32_MAX	0xFFFFFFFFUL
#define UINT16_MAX	0xFFFFU
#define UINT8_MAX	0xFFU

#endif
