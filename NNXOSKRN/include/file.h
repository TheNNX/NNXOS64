#ifndef NNX_FILE_HEADER
#define NNX_FILE_HEADER

#include <nnxtype.h>

typedef struct _IO_STATUS_BLOCK
{
    union
    {
        NTSTATUS Status;
        PVOID    Pointer;
    };
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

#define FILE_SUPERSEDED             0x00000000
#define FILE_OPENED                 0x00000001
#define FILE_CREATED                0x00000002
#define FILE_OVERWRITTEN            0x00000003
#define FILE_EXISTS                 0x00000004
#define FILE_DOES_NOT_EXIST         0x00000005

#define FILE_ATTRIBUTE_NORMAL       0x80
#define FILE_ATTRIBUTE_ARCHIVE      0x20
#define FILE_ATTRIBUTE_SYSTEM       0x4
#define FILE_ATTRIBUTE_HIDDEN       0x2
#define FILE_ATTRIBUTE_READONLY     0x1

#define FILE_SUPERSEDE              0x00000000
#define FILE_OPEN                   0x00000001
#define FILE_CREATE                 0x00000002
#define FILE_OPEN_IF                0x00000003
#define FILE_OVERWRITE              0x00000004
#define FILE_OVERWRITE_IF           0x00000005

#define FILE_SHARE_READ             0x00000001
#define FILE_SHARE_WRITE            0x00000002
#define FILE_SHARE_DELETE           0x00000004

#define FILE_READ_DATA              1
#define FILE_LIST_DIRECTORY         1
#define FILE_ADD_FILE               2
#define FILE_WRITE_DATA             2
#define FILE_ADD_SUBDIRECTORY       4
#define FILE_APPEND_DATA            4
#define FILE_CREATE_PIPE_INSTANCE   4
#define FILE_READ_EA                8
#define FILE_WRITE_EA               16
#define FILE_EXECUTE                32
#define FILE_TRAVERSE               32
#define FILE_DELETE_CHILD           64
#define FILE_READ_ATTRIBUTES        128
#define FILE_WRITE_ATTRIBUTES       256

#define FILE_DIRECTORY_FILE         0x00000001
#define FILE_NON_DIRECTORY_FILE     0x00000040
#define FILE_DELETE_ON_CLOSE        0x00001000

#define FILE_GENERIC_READ  (STANDARD_RIGHTS_READ | \
                            FILE_READ_DATA | \
                            FILE_READ_ATTRIBUTES | \
                            FILE_READ_EA | \
                            SYNCHRONIZE)

#define FILE_GENERIC_WRITE (STANDARD_RIGHTS_WRITE |\
                            FILE_WRITE_DATA |\
                            FILE_WRITE_ATTRIBUTES |\
                            FILE_WRITE_EA |\
                            FILE_APPEND_DATA |\
                            SYNCHRONIZE)

#define FILE_GENERIC_EXECUTE (STANDARD_RIGHTS_EXECUTE |\
                              FILE_READ_ATTRIBUTES |\
                              FILE_EXECUTE |\
                              SYNCHRONIZE)

#ifdef __cplusplus
extern "C"{
#endif

    NTSTATUS
    NTAPI
    NtFileObjInit(VOID);

    NTSYSAPI
    NTSTATUS
    NTAPI
    NtCreateFile(
        PHANDLE pOutHandle,
        ACCESS_MASK DesiredAccessMask,
        POBJECT_ATTRIBUTES pInObjectAttributes,
        PIO_STATUS_BLOCK IoStatusBlock,
        PLARGE_INTEGER AllocationSize,
        ULONG FileAttributes,
        ULONG ShareAccess,
        ULONG CreateDisposition,
        ULONG CreateOptions,
        PVOID ExtAttributesBuffer,
        ULONG EaBufferSize);

    NTSTATUS
    NTAPI
    NnxGetNtFileSize(
        HANDLE hFile, 
        PLARGE_INTEGER outSize);

    NTSTATUS
    NTAPI
    NnxGetImageSection(
        HANDLE hFile,
        PHANDLE outSection);

    NTSTATUS
    NTAPI
    NnxSetImageSection(
        HANDLE hFile,
        HANDLE hSection);

    NTSYSAPI
    NTSTATUS
    NTAPI
    NtReadFile(
        HANDLE hFile,
        HANDLE hEvent,
        /* Reserved */
        PVOID pApcRoutine,
        /* Reserved */
        PVOID pApcContext,
        PIO_STATUS_BLOCK pStatusBlock,
        PVOID Buffer,
        ULONG Length,
        PLARGE_INTEGER ByteOffset,
        PULONG Key);
#ifdef __cplusplus
}
#endif

#endif