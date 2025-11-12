#pragma once

#include <rtlstring.h>
#include <nnxtype.h>
#include <irp.h>

typedef struct _DEVICE_OBJECT DEVICE_OBJECT, *PDEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT, *PDRIVER_OBJECT;

typedef NTSTATUS (DRIVER_INITIALIZE)(PDRIVER_OBJECT DriverObject,
                                     PUNICODE_STRING RegistryPath);
typedef DRIVER_INITIALIZE *PDRIVER_INITIALIZE;

typedef NTSTATUS (DRIVER_STARTIO)(PDRIVER_OBJECT DriverObject,
                                  PIRP Irp);
typedef DRIVER_STARTIO* PDRIVER_STARTIO;

typedef NTSTATUS(DRIVER_UNLOAD)(PDRIVER_OBJECT DriverObject);
typedef DRIVER_UNLOAD* PDRIVER_UNLOAD;

typedef NTSTATUS(*PDRIVER_ADD_DEVICE)(PDRIVER_OBJECT DriverObject, 
                                      PDEVICE_OBJECT DeviceObject);

typedef struct _FAST_IO_DISPATCH
{
    ULONG   SizeOfFastIoDispatch;
    PVOID   Reserved[27];
} FAST_IO_DISPATCH, *PFAST_IO_DISPATCH;

typedef NTSTATUS (DRIVER_DISPATCH)(struct _DRIVER_OBJECT* DriverObject, PIRP Irp);
typedef DRIVER_DISPATCH *PDRIVER_DISPATCH;

typedef struct _DRIVER_EXTENSION DRIVER_EXTENSION, *PDRIVER_EXTENSION;

struct _DEVICE_OBJECT
{
    CSHORT          Type;
    USHORT          Size;
    LONG            ReferenceCount;
    PDRIVER_OBJECT  DriverObject;
    PDEVICE_OBJECT  NextDevice;
    PDEVICE_OBJECT  AttachedDevice;
    PIRP            Irp;
};

struct _DRIVER_OBJECT
{
    CSHORT             Type;
    CSHORT             Size;
    PDEVICE_OBJECT     DeviceObject;
    ULONG              Flags;
    PVOID              DriverStart;
    ULONG              DriverSize;
    PVOID              DriverSection;
    PDRIVER_EXTENSION  DriverExtension;
    UNICODE_STRING     DriverName;
    PUNICODE_STRING    HardwareDatabase;
    PFAST_IO_DISPATCH  FastIoDispatch;
    PDRIVER_INITIALIZE DriverInit;
    PDRIVER_STARTIO    DriverStartIo;
    PDRIVER_UNLOAD     DriverUnload;
    PDRIVER_DISPATCH   MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
};

struct _DRIVER_EXTENSION
{
    PDRIVER_OBJECT DriverObject;
    PDRIVER_ADD_DEVICE AddDevice;
};