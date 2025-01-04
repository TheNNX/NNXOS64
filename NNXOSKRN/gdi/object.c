#include <gdi.h>
#include <pool.h>
#include <rtl.h>
#include <ntdebug.h>

#include <SimpleTextIO.h>

static PGDI_OBJECT_HEADER* ObjectDatabase = NULL;
static SIZE_T ObjectDatabaseLength = 0;
static KSPIN_LOCK ObjectDatabaseLock;

extern KSPIN_LOCK GdiDisplayDevicesLock;
extern LIST_ENTRY GdiDisplayDevicesHead;

static GDI_HANDLE GenericGdiDisplayDevice = NULL;

NTSTATUS 
NTAPI
GdiInit(
    SIZE_T maxGdiObjects)
{
    SIZE_T i;

    PrintT("Initializing GDI\n");

    InitializeListHead(&GdiDisplayDevicesHead);
    KeInitializeSpinLock(&GdiDisplayDevicesLock);

    KeInitializeSpinLock(&ObjectDatabaseLock);

    /* Create and initialize the object database. */
    ObjectDatabase = 
        ExAllocatePoolWithTag(
            PagedPool, 
            maxGdiObjects * sizeof(*ObjectDatabase),
            'GDI ');
    if (ObjectDatabase == NULL)
    {
        PrintT(
            "["__FILE__":%d] "__FUNCTION__" failed, no memory to alloc %i(%i) objects\n", 
            __LINE__,
            maxGdiObjects,
            maxGdiObjects * sizeof(ObjectDatabase));

        return STATUS_NO_MEMORY;
    }
    ObjectDatabaseLength = maxGdiObjects;
    
    for (i = 0; i < maxGdiObjects; i++)
    {
        ObjectDatabase[i] = NULL;
    }

    GenericGdiDisplayDevice = GdiCreateDevice(L"DISPLAY");

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
GdiUnlockHandle(
    GDI_HANDLE handle, 
    PGDI_OBJECT_HEADER object)
{
    KIRQL irql;
    if (handle == NULL || object == GDI_LOCKED_OBJECT)
    {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&ObjectDatabaseLock, &irql);
    if (ObjectDatabase[handle - 1] != GDI_LOCKED_OBJECT)
    {
        PrintT("Warning - invalid handle unlock for %X with %X\n", handle, object);
        KeReleaseSpinLock(&ObjectDatabaseLock, irql);
        return STATUS_INVALID_PARAMETER;
    }

    if (object && object->SelfHandle != handle)
    {
        PrintT("Warning - %X->SelfHandle == %X, not %X\n", object, object->SelfHandle, handle);
    }
    ObjectDatabase[handle - 1] = object;
    KeReleaseSpinLock(&ObjectDatabaseLock, irql);

    return STATUS_SUCCESS;
}

PGDI_OBJECT_HEADER 
NTAPI 
GdiLockHandle(
    GDI_HANDLE handle)
{
    KIRQL irql;
    PGDI_OBJECT_HEADER result;

    if (handle == NULL)
    {
        return NULL;
    }

    do
    {
        KeAcquireSpinLock(&ObjectDatabaseLock, &irql);
        result = ObjectDatabase[handle - 1];
        if (result == GDI_LOCKED_OBJECT)
        {
            KeReleaseSpinLock(&ObjectDatabaseLock, irql);
        }
    } 
    while (result == GDI_LOCKED_OBJECT);

    ObjectDatabase[handle - 1] = GDI_LOCKED_OBJECT;
    KeReleaseSpinLock(&ObjectDatabaseLock, irql);

    return result;
}

VOID
NTAPI
GdiDestroy(
    GDI_HANDLE handle)
{
    PGDI_OBJECT_HEADER pObj;
    
    if (handle == NULL)
    {
        return;
    }

    pObj = GdiLockHandle(handle);
    if (pObj)
    {
        GdiFreeObject(pObj);
    }

    GdiUnlockHandle(handle, NULL);
}

GDI_HANDLE 
NTAPI
GdiRegisterObject(PGDI_OBJECT_HEADER pObject)
{
    GDI_HANDLE i;
    KIRQL irql;

    for (i = 0; i < ObjectDatabaseLength; i++)
    {
        KeAcquireSpinLock(&ObjectDatabaseLock, &irql);
        if (ObjectDatabase[i] == NULL)
        {
            ObjectDatabase[i] = pObject;
            pObject->SelfHandle = i + 1;
            KeInitializeSpinLock(&pObject->Lock);

            KeReleaseSpinLock(&ObjectDatabaseLock, irql);
            return i + 1;
        }
        KeReleaseSpinLock(&ObjectDatabaseLock, irql);
    }

    return NULL;
}

PGDI_OBJECT_HEADER
NTAPI
GdiCreateObject(
    GDI_OBJECT_TYPE type,
    SIZE_T size)
{
    PGDI_OBJECT_HEADER object = ExAllocatePoolWithTag(
        PagedPool, size, 'GDIO');

    if (object == NULL)
    {
        return NULL;
    }

    object->Type = type;
    object->Destructor = NULL;
    KeInitializeSpinLock(&object->Lock);

    return object;
}

VOID
NTAPI
GdiMoveIntoHandle(
    GDI_HANDLE Dst,
    GDI_HANDLE Src)
{
    KIRQL irql;
    PGDI_OBJECT_HEADER pObjDst, pObjSrc;

    if (Dst == NULL || Src == NULL)
    {
        return;
    }

    pObjDst = GdiLockHandle(Dst);
    pObjSrc = GdiLockHandle(Src);

    if (pObjSrc == NULL)
    {
        GdiUnlockHandle(Dst, pObjDst);
        return;
    }

    if (pObjDst != NULL)
    {
        /* Destroy the object that used to be under handle Dst */
        GdiFreeObject(pObjDst);
    }

    /* pObjSrc is locked now, set its self handle to Dst */
    pObjSrc->SelfHandle = Dst;

    /* Unlock the Dst handle with the pointer to Src */
    GdiUnlockHandle(Dst, pObjSrc);

    /* Free the Src handle */
    KeAcquireSpinLock(&ObjectDatabaseLock, &irql);
    ObjectDatabase[Src - 1] = NULL;
    KeReleaseSpinLock(&ObjectDatabaseLock, irql);
}

VOID
NTAPI
GdiMovePtrIntoHandle(
    GDI_HANDLE Dst,
    PGDI_OBJECT_HEADER SrcPtr)
{
    KIRQL irql;
    GDI_HANDLE Src;

    PGDI_OBJECT_HEADER pObjDst;

    if (Dst == NULL || SrcPtr == NULL)
    {
        return;
    }

    pObjDst = GdiLockHandle(Dst);
    if (pObjDst != NULL)
    {
        GdiFreeObject(pObjDst);
    }

    SrcPtr->SelfHandle = Dst;
    GdiUnlockHandle(Dst, SrcPtr);

    KeAcquireSpinLock(&ObjectDatabaseLock, &irql);
    Src = SrcPtr->SelfHandle;
    if (Src != NULL)
    {
        ObjectDatabase[Src - 1] = NULL;
    }
    KeReleaseSpinLock(&ObjectDatabaseLock, irql);

}

VOID
NTAPI
GdiFreeObject(
    PGDI_OBJECT_HEADER pObject)
{
    if (pObject == NULL)
    {
        return;
    }

    if (pObject->Destructor != NULL)
    {
        pObject->Destructor(pObject);
    }

    ExFreePoolWithTag(pObject, 'GDIO');
}