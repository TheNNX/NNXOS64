#ifndef NNX_OBJECT_HEADER
#define NNX_OBJECT_HEADER

#include <nnxtype.h>
#include <ntlist.h>
#include "HAL/cpu.h"

#ifdef __cplusplus
extern "C" {
#endif
	typedef enum KWAIT_REASON
	{
		Executive = 0
	}KWAIT_REASON;

	typedef enum _WAIT_TYPE
	{
		WaitAll,
		WaitAny
	}WAIT_TYPE;

	typedef struct _DISPATCHER_HEADER
	{
		union
		{
			/*struct
			{
				UCHAR Padding[3];
				UCHAR Type;
			};*/
			KSPIN_LOCK Lock;
		};
		LONG SignalState;
		LIST_ENTRY WaitHead;
	}DISPATCHER_HEADER;

	typedef struct _KWAIT_BLOCK
	{
		LIST_ENTRY WaitEntry;
		KPROCESSOR_MODE WaitMode;
		UCHAR WaitType;
		struct _KTHREAD* Thread;
		DISPATCHER_HEADER* Object;
	}KWAIT_BLOCK, *PKWAIT_BLOCK;

	inline VOID InitializeDispatcherHeader(DISPATCHER_HEADER* Header, UCHAR Type)
	{
		KeInitializeSpinLock(&Header->Lock);
		// Header->Type = Type;
		Header->SignalState = 0;
		InitializeListHead(&Header->WaitHead);
	}

	NTSTATUS KeWaitForSingleObject(PVOID Object, KWAIT_REASON WaitReason, KPROCESSOR_MODE WaitMode, BOOL Alertable, PLONG64 Timeout);
	
	NTSTATUS KeWaitForMultipleObjects(
		ULONG Count,
		PVOID *Object,
		WAIT_TYPE WaitType,
		KWAIT_REASON WaitReason,
		KPROCESSOR_MODE WaitMode,
		BOOLEAN Alertable,
		PLONG64 Timeout,
		PKWAIT_BLOCK WaitBlockArray
	);

	VOID KiHandleObjectWaitTimeout(struct _KTHREAD* Thread, PLONG64 pTimeout, BOOL Alertable);

#define OBJECT_TYPE_INVALID		0
#define OBJECT_TYPE_KQUEUE		1
#define OBJECT_TYPE_KPROCESS	2
#define OBJECT_TYPE_KTHREAD		3
#define OBJECT_TYPE_TIMER		4

	typedef struct _UNICODE_STRING* PUNICODE_STRING;

	typedef struct _OBJECT_TYPE
	{
		LIST_ENTRY ListHeader;
		PUNICODE_STRING Name;
		ULONG NumberOfObjects;
	}OBJECT_TYPE, *POBJECT_TYPE;

	typedef struct _OBJECT_HANDLE *HANDLE;

	typedef struct _OBJECT_ATTRIBUTES
	{
		ULONG Length;
		HANDLE RootDirectory;
		PUNICODE_STRING ObjectName;
		ULONG Attributes;
		PVOID SecurityDescriptor;
		PVOID Unused;
	}OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

	typedef struct _OBJECT_HANDLE
	{
		PVOID Object;
	}OBJECT_HANDLE, *POBJECT_HANDLE;

	typedef struct _OBJECT_HEADER
	{
		ULONG ReferenceCount;
		ULONG HandleCount;
		POBJECT_TYPE ObjectType;
		OBJECT_ATTRIBUTES Attributes;
		LIST_ENTRY_POINTER Children;
		KSPIN_LOCK Lock;
	}OBJECT_HEADER, * POBJECT_HEADER;

	typedef ULONG_PTR ACCESS_MASK;

#define OBJ_INHERIT            0x00000002L
#define OBJ_PERMANENT          0x00000010L
#define OBJ_EXCLUSIVE          0x00000020L
#define OBJ_CASE_INSENSITIVE   0x00000040L
#define OBJ_OPENIF             0x00000080L
#define OBJ_OPENLINK           0x00000100L
#define OBJ_KERNEL_HANDLE      0x00000200L
#define OBJ_FORCE_ACCESS_CHECK 0x00000400L

#define STATUS_OBJECT_TYPE_MISMATCH 0xC0000024

	VOID NTAPI ObReferenceObject(
		PVOID Object
	);

	NTSTATUS NTAPI ObReferenceObjectByPointer(
		PVOID Object,
		ACCESS_MASK AccessMask,
		POBJECT_TYPE ObjectType,
		KPROCESSOR_MODE AccessMode
	);

	NTSTATUS NTAPI ObReferenceObjectByHandle(
		HANDLE ObjectHandle, 
		ACCESS_MASK AccessMask, 
		POBJECT_TYPE ObjectType, 
		KPROCESSOR_MODE AccessMode, 
		PVOID* Object, 
		PVOID HandleInformation
	);

	BOOL ObpCheckObjectForDeletion(PVOID Object);

	VOID NTAPI ObDereferenceObject(PVOID Object);

	NTSTATUS ZwClose(HANDLE Handle);

	VOID ObpDeleteObject(PVOID Object);

	NTSTATUS ObpCreateObject(
		KPROCESSOR_MODE Unused1,
		POBJECT_TYPE ObjectType, 
		POBJECT_ATTRIBUTES Attributes, 
		KPROCESSOR_MODE AccessMode,
		PVOID Unused2,
		ULONG ObjectSize, 
		ULONG PagedPoolCharge,
		ULONG NonPagedPoolCharge,
		PVOID* Object
	);

	NTSTATUS ObpInsertObject(PVOID Object);
	
	extern POBJECT_TYPE ObjTypeObjType;

#ifdef __cplusplus
}
#endif

#endif