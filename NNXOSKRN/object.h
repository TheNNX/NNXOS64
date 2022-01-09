#ifndef NNX_OBJECT_HEADER
#define NNX_OBJECT_HEADER

#include <nnxtype.h>
#include <ntlist.h>
#include "HAL/X64/cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef ULONG_PTR	HANDLE;
	typedef ULONG		ACCESS_MASK;
	typedef struct _OBJECT_TYPE *POBJECT_TYPE;
	typedef struct _OBJECT_HANDLE_INFORMATION *POBJECT_HANDLE_INFORMATION;

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

#ifdef __cplusplus
}
#endif

#endif