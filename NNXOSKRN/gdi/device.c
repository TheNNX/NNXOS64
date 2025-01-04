#include <gdi.h>
#include <pool.h>
#include <rtl.h>
#include <ntdebug.h>

LIST_ENTRY GdiDisplayDevicesHead;
KSPIN_LOCK GdiDisplayDevicesLock;

static BOOLEAN CheckWszsEqual(LPCWSTR wsz1, LPCWSTR wsz2)
{
    if (wsz1 == wsz2)
    {
        return TRUE;
    }
    
    if (wsz1 == NULL || wsz2 == NULL)
    {
        return FALSE;
    }

    while (*wsz1 && *wsz2)
    {
        if (*wsz1++ != *wsz2++)
        {
            return FALSE;
        }
    }
    return (*wsz1 == *wsz2);
}

GDI_HANDLE
NTAPI
GdiOpenDevice(PCWSTR wszName)
{
    KIRQL irql;
    PLIST_ENTRY current;
    GDI_HANDLE result = NULL;

    KeAcquireSpinLock(&GdiDisplayDevicesLock, &irql);

    current = GdiDisplayDevicesHead.First;
    while (current != &GdiDisplayDevicesHead)
    {
        PGDI_DISPLAY_DEVICE curDevice =
            CONTAINING_RECORD(current, GDI_DISPLAY_DEVICE, ListEntry);

        KeAcquireSpinLockAtDpcLevel(&curDevice->Lock);

        if (CheckWszsEqual(wszName, curDevice->Name))
        {
            result = curDevice->SelfHandle;
            KeReleaseSpinLockFromDpcLevel(&curDevice->Lock);
            break;
        }

        KeReleaseSpinLockFromDpcLevel(&curDevice->Lock);

        current = current->Next;
    }

    KeReleaseSpinLock(&GdiDisplayDevicesLock, irql);
    return result;
}

GDI_HANDLE
NTAPI
GdiCreateDevice(PCWSTR wszName)
{
    SIZE_T len;
    GDI_HANDLE result = NULL;
    PGDI_DISPLAY_DEVICE displayDevice = NULL;
    
    displayDevice = (PGDI_DISPLAY_DEVICE)GdiCreateObject(
        GDI_OBJECT_DEVICE_TYPE,
        sizeof(GDI_DISPLAY_DEVICE));
        
    ASSERT(wszName != NULL);

    len = 0;
    while (wszName[len++] != '\0');
    if (len > 31)
    {
        len = 31;
    }

    RtlCopyMemory(displayDevice->Name, wszName, len);
    displayDevice->Name[31] = L'\0';

    result = GdiRegisterObject((PGDI_OBJECT_HEADER)displayDevice);
    if (result == NULL)
    {
        ExFreePool((PGDI_OBJECT_HEADER)displayDevice);
    }

    ExInterlockedInsertTailList(
        &GdiDisplayDevicesHead, 
        &displayDevice->ListEntry, 
        &GdiDisplayDevicesLock);

    return result;
}