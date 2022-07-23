#ifndef NNX_OBJECT_HEADER
#define NNX_OBJECT_HEADER

#include <nnxtype.h>
#include <ntlist.h>
#include "handle.h"
#include "namespace.h"
#include <rtl/rtlstring.h>
#include <HAL/cpu.h>

#define OBJ_INHERIT            0x00000002L
#define OBJ_PERMANENT          0x00000010L
#define OBJ_EXCLUSIVE          0x00000020L
#define OBJ_CASE_INSENSITIVE   0x00000040L
#define OBJ_OPENIF             0x00000080L
#define OBJ_OPENLINK           0x00000100L
#define OBJ_KERNEL_HANDLE      0x00000200L
#define OBJ_FORCE_ACCESS_CHECK 0x00000400L
#define OBJ_VALID_ATTRIBUTES   0x000007F2L

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
        KSPIN_LOCK Lock;
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
        Header->SignalState = 0;
        InitializeListHead(&Header->WaitHead);
    }

    NTSTATUS 
        KeWaitForSingleObject(
            PVOID Object,
            KWAIT_REASON WaitReason, 
            KPROCESSOR_MODE WaitMode,
            BOOL Alertable,
            PLONG64 Timeout
        );

    NTSTATUS 
        KeWaitForMultipleObjects(
            ULONG Count,
            PVOID *Object,
            WAIT_TYPE WaitType,
            KWAIT_REASON WaitReason,
            KPROCESSOR_MODE WaitMode,
            BOOLEAN Alertable,
            PLONG64 Timeout,
            PKWAIT_BLOCK WaitBlockArray
        );

    VOID
        KeUnwaitThread(
            struct _KTHREAD* pThread,
            LONG_PTR WaitStatus,
            LONG PriorityIncrement
        );

    VOID KiHandleObjectWaitTimeout(struct _KTHREAD* Thread, PLONG64 pTimeout, BOOL Alertable);

#define OBJECT_TYPE_INVALID		0
#define OBJECT_TYPE_KQUEUE		1
#define OBJECT_TYPE_KPROCESS	2
#define OBJECT_TYPE_KTHREAD		3
#define OBJECT_TYPE_TIMER		4


    typedef DWORD ACCESS_MASK;

    typedef struct _OBJECT_TYPE
    {
        NTSTATUS(*ObjectOpen)(
            PVOID SelfObject, 
            PVOID* pOutObject, 
            ACCESS_MASK DesiredAccess, 
            KPROCESSOR_MODE AcessMode,
            PUNICODE_STRING Name,
            BOOL CaseInsensitive
        );

        NTSTATUS(*AddChildObject)(
            PVOID SelfObject,
            PVOID Child
        );

        NTSTATUS(*OnOpen)(PVOID SelfObject);
        NTSTATUS(*OnClose)(PVOID SelfObject);

        NTSTATUS(*EnumerateChildren)(
            PVOID SelfObject, 
            PLIST_ENTRY CurrentEntry
        );

        NTSTATUS(*OnCreate)(PVOID SelfObject, PVOID data);
        NTSTATUS(*OnDelete)(PVOID SelfObject);
    }OBJECT_TYPE, *POBJECT_TYPE;

    typedef struct _OBJECT_HEADER
    {
        LIST_ENTRY ParentChildListEntry;
        UNICODE_STRING Name;
        HANDLE Root;
        ULONG Attributes;
        ACCESS_MASK Access;
        ULONG ReferenceCount;
        ULONG HandleCount;
        LIST_ENTRY HandlesHead;
        POBJECT_TYPE ObjectType;
        KSPIN_LOCK Lock;
    }OBJECT_HEADER, *POBJECT_HEADER;

    typedef struct _OBJECT_ATTRIBUTES
    {
        ULONG Length;
        HANDLE Root;
        PUNICODE_STRING ObjectName;
        ULONG Attributes;
        PVOID SecurityDescriptor;
        PVOID Unused;
    }OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

    NTSTATUS ObCreateObject(
        PVOID* pObject, 
        ACCESS_MASK DesiredAccess, 
        KPROCESSOR_MODE AccessMode, 
        POBJECT_ATTRIBUTES Attributes,
        ULONG ObjectSize,
        POBJECT_TYPE ObjectType,
        PVOID OptionalData
    );

    NTSTATUS ObReferenceObjectByHandle(
        HANDLE handle,
        ACCESS_MASK desiredAccess,
        POBJECT_TYPE objectType,
        KPROCESSOR_MODE accessMode,
        PVOID* pObject,
        PVOID unused
    );

    NTSTATUS ObReferenceObject(PVOID object);

    NTSTATUS ObReferenceObjectByPointer(
        PVOID object,
        ACCESS_MASK desiredAccess,
        POBJECT_TYPE objectType,
        KPROCESSOR_MODE accessMode
    );

    NTSTATUS ObDereferenceObject(PVOID object);

    NTSTATUS ObInit();

    inline VOID InitializeObjectAttributes(
        POBJECT_ATTRIBUTES pAttributes,
        PUNICODE_STRING name,
        ULONG flags,
        HANDLE root,
        PVOID security
    )
    {
        pAttributes->Attributes = flags;
        pAttributes->ObjectName = name;
        if (root == NULL)
            root = INVALID_HANDLE_VALUE;
        pAttributes->Root = root;
        pAttributes->SecurityDescriptor = security;
        pAttributes->Length = sizeof(*pAttributes);
        pAttributes->Unused = NULL;
    }

    inline POBJECT_HEADER ObGetHeaderFromObject(PVOID Object)
    {
        return (POBJECT_HEADER)((ULONG_PTR)Object - sizeof(OBJECT_HEADER));
    }

    inline PVOID ObGetObjectFromHeader(POBJECT_HEADER Header)
    {
        return (PVOID)((ULONG_PTR)Header + sizeof(OBJECT_HEADER));
    }

    NTSTATUS ObCreateSchedulerTypes(
        POBJECT_TYPE* poutProcessType,
        POBJECT_TYPE* poutThreadType
    );

    NTSTATUS ObChangeRoot(
        PVOID object, 
        HANDLE newRoot, 
        KPROCESSOR_MODE accessMode
    );

    extern POBJECT_TYPE ObTypeObjectType;
    extern POBJECT_TYPE ObDirectoryObjectType;
    extern POBJECT_TYPE PsProcessType;
    extern POBJECT_TYPE PsThreadType;

#ifdef __cplusplus
}
#endif

#endif