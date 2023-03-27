#ifndef NNX_OBJECT_HEADER
#define NNX_OBJECT_HEADER

#include <nnxtype.h>
#include <ntlist.h>
#include "handle.h"
#include "namespace.h"
#include <rtlstring.h>
#include <cpu.h>

#define OBJ_INHERIT             0x00000002L
#define OBJ_PERMANENT           0x00000010L
#define OBJ_EXCLUSIVE           0x00000020L
#define OBJ_CASE_INSENSITIVE    0x00000040L
#define OBJ_OPENIF              0x00000080L
#define OBJ_OPENLINK            0x00000100L
#define OBJ_KERNEL_HANDLE       0x00000200L
#define OBJ_FORCE_ACCESS_CHECK  0x00000400L
#define OBJ_VALID_ATTRIBUTES    0x000007F2L

#define DELETE                  0x00010000L
#define READ_CONTROL            0x00020000L
#define WRITE_DAC               0x00040000L
#define WRITE_OWNER             0x00080000L
#define SYNCHRONIZE             0x00100000L
#define STANDARD_RIGHTS_READ    READ_CONTROL
#define STANDARD_RIGHTS_WRITE   READ_CONTROL
#define STANDARD_RIGHTS_EXECUTE READ_CONTROL
#define STANDARD_RIGHTS_ALL     0x001F0000L
#define SPECIFIC_RIGHTS_ALL     0x0000FFFFL

#ifdef __cplusplus
extern "C" {
#endif

    typedef DWORD ACCESS_MASK;

    typedef struct _OBJECT_TYPE OBJECT_TYPE, *POBJECT_TYPE;

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


    inline VOID InitializeObjectAttributes(
        POBJECT_ATTRIBUTES pAttributes,
        PUNICODE_STRING name,
        ULONG flags,
        HANDLE root,
        PVOID security)
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

    NTSYSAPI
    NTSTATUS
    NTAPI
    ObReferenceObjectByHandle(
        HANDLE handle,
        ACCESS_MASK desiredAccess,
        POBJECT_TYPE objectType,
        KPROCESSOR_MODE accessMode,
        PVOID* pObject,
        PVOID unused);

    NTSYSAPI
    NTSTATUS
    NTAPI
    ObReferenceObject(PVOID object);

    NTSYSAPI
    NTSTATUS
    NTAPI
    ObReferenceObjectByPointer(
        PVOID object,
        ACCESS_MASK desiredAccess,
        POBJECT_TYPE objectType,
        KPROCESSOR_MODE accessMode);

    NTSYSAPI
    NTSTATUS
    NTAPI
    ObDereferenceObject(PVOID object);

#ifdef NNX_KERNEL
    struct _OBJECT_TYPE
    {
        NTSTATUS(*ObjectOpen)(
            PVOID SelfObject,
            PVOID* pOutObject,
            ACCESS_MASK DesiredAccess,
            KPROCESSOR_MODE AcessMode,
            PUNICODE_STRING Name,
            BOOL CaseInsensitive);

        NTSTATUS(*AddChildObject)(
            PVOID SelfObject,
            PVOID Child);

        NTSTATUS(*OnOpen)(
            PVOID SelfObject);

        NTSTATUS(*OnClose)(
            PVOID SelfObject);

        NTSTATUS(*EnumerateChildren)(
            PVOID SelfObject,
            PLIST_ENTRY CurrentEntry);

        NTSTATUS(*OnCreate)(
            PVOID SelfObject, 
            PVOID data);

        NTSTATUS(*OnDelete)(
            PVOID SelfObject);

        SIZE_T InstanceSize;
    };

    NTSTATUS 
    NTAPI    
    ObCreateObject(
        PVOID* pObject, 
        ACCESS_MASK DesiredAccess, 
        KPROCESSOR_MODE AccessMode, 
        POBJECT_ATTRIBUTES Attributes,
        POBJECT_TYPE ObjectType,
        PVOID OptionalData);

    NTSTATUS 
    NTAPI    
    ObInit();

    NTSTATUS
    NTAPI
    ObCreateType(
        POBJECT_TYPE* pOutObjectType,
        PUNICODE_STRING TypeName,
        SIZE_T InstanceSize);

    NTSTATUS 
    NTAPI
    ObChangeRoot(
        PVOID object, 
        HANDLE newRoot, 
        KPROCESSOR_MODE accessMode);

    extern POBJECT_TYPE ObTypeObjectType;
    extern POBJECT_TYPE ObDirectoryObjectType;
    extern POBJECT_TYPE PsProcessType;
    extern POBJECT_TYPE PsThreadType;

#endif

#ifdef __cplusplus
}
#endif

#endif