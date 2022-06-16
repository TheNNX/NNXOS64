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

typedef INT64	LONG64, *PLONG64, *LPLONG64;

typedef ULONG NTSTATUS;
typedef LONG INT;
typedef ULONG UINT;

#define STATUS_SUCCESS				    0x00000000UL
#define STATUS_ABANDONED		    	0x00000080UL
#define STATUS_USER_APC				    0x000000C0UL
#define STATUS_ALERTED				    0x00000101UL
#define STATUS_TIMEOUT				    0x00000102UL
#define STATUS_INVALID_PARAMETER	    0xC000000DUL
#define STATUS_NO_MEMORY			    0xC0000017UL

#define STATUS_INVALID_HANDLE           0xC0000008UL
#define STATUS_OBJECT_TYPE_MISMATCH     0xC0000024UL
#define STATUS_OBJECT_NAME_NOT_FOUND    0xC0000034UL
#define STATUS_OBJECT_NAME_COLLISION    0xC0000035UL
#define STATUS_OBJECT_PATH_INVALID      0xC0000039UL
#define STATUS_ACCESS_DENIED            0xC0000022UL

#ifndef _M_AMD64
typedef ULONG ULONG_PTR;
typedef LONG LONG_PTR;
#else
typedef ULONGLONG ULONG_PTR;
typedef LONGLONG LONG_PTR;
#endif

typedef ULONG_PTR SIZE_T;

#ifndef WCHAR_MAX
typedef UINT16	WCHAR;
#define WCHAR_MAX 65535U

typedef WCHAR *PWSTR, *LPWSTR;
typedef const WCHAR *PCWSTR, *LPCWSTR;

typedef const char CCHAR, *PCCHAR;
typedef const unsigned char CUCHAR, *PCUCHAR;
typedef const short CSHORT, *PCSHORT;
typedef const unsigned short CUSHORT, *PCUSHORT;
typedef const char CLONG, *PCLONG;
typedef const unsigned char CULONG, *PCULONG;
typedef const short CLONGLONG, *PCLONGLONG;
typedef const unsigned short CULONGLONG, *PCULONGLONG;

#endif

#define TRUE 1
#define FALSE 0

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
#define NULL 0
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

#define INT64_MAX	0x7FFFFFFFFFFFFFFFLL
#define INT32_MAX	0x7FFFFFFFL
#define INT16_MAX	0x7FFF
#define INT8_MAX	0x7F

#endif
