#ifndef NNX_IRP_HEADER
#define NNX_IRP_HEADER

#include <nnxtype.h>
#include <cpu.h>
#include <irql.h>
#include <ntlist.h>
#include <scheduler.h>
#include <mdl.h>
#include <apc.h>

#define IRP_MJ_MAXIMUM_FUNCTION 0

#ifdef __cplusplus
extern "C" {
#endif
    typedef struct _IO_STATUS_BLOCK
    {
        union
        {
            NTSTATUS Status;
            PVOID Pointer;
        };
        ULONG_PTR Information;
    } IO_STATUS_BLOCK, * PIO_STATUS_BLOCK;

    typedef struct _IRP
    {
        CSHORT Type;
        USHORT Size;
        PMDL MdlAddress;
        ULONG Flags;
        union
        {
            struct _IRP* MasterIrp;
            volatile LONG IrpCount;
            PVOID SystemBuffer;
        } AssociatedIrp;
        LIST_ENTRY ThreadListEntry;
        IO_STATUS_BLOCK IoStatus;
        KPROCESSOR_MODE RequestorMode;
        BOOLEAN PendingReturned;
        CHAR StackCount;
        CHAR CurrentLocation;
        BOOLEAN Cancel;
        KIRQL CancelIrql;
        CCHAR ApcEnvironment;
        UCHAR AllocationFlags;
        union
        {
            PIO_STATUS_BLOCK UserIosb;
            PVOID IoRingContext;
        };
#if 0
        PKEVENT UserEvent;
        union
        {
            struct
            {
                union
                {
                    PIO_APC_ROUTINE UserApcRoutine;
                    PVOID IssuingProcess;
                };
                union
                {
                    PVOID UserApcContext;
                    struct _IORING_OBJECT* IoRing;
                };
            } AsynchronousParameters;
            LARGE_INTEGER AllocationSize;
        } Overlay;
        volatile PDRIVER_CANCEL CancelRoutine;
#endif
        PVOID UserBuffer;
#if 0
        union
        {
            struct
            {
                union
                {
                    KDEVICE_QUEUE_ENTRY DeviceQueueEntry;
                    struct
                    {
                        PVOID DriverContext[4];
                    };
                };
                PETHREAD Thread;
                PCHAR AuxiliaryBuffer;
                struct
                {
                    LIST_ENTRY ListEntry;
                    union
                    {
                        struct _IO_STACK_LOCATION* CurrentStackLocation;
                        ULONG PacketType;
                    };
                };
                PFILE_OBJECT OriginalFileObject;
            } Overlay;
            KAPC Apc;
            PVOID CompletionKey;
        } Tail;
#endif
    } IRP, *PIRP;
#ifdef __cplusplus
}
#endif

#endif