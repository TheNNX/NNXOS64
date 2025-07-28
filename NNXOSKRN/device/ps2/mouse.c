#include <ps2.h>
#include <interrupt.h>
#include <HALX64/include/APIC.h>
#include <cpu.h>
#include <dpc.h>
#include <gdi.h>

#include <ntsemaphore.h>
#include <SimpleTextIO.h>

static SHORT MouseX = 0;
static SHORT MouseY = 0;
static BOOLEAN MouseLeft = FALSE, MouseMiddle = FALSE, MouseRight = FALSE;
static KSEMAPHORE MouseSemaphore;

static KDPC MouseDpc;

#define MOUSE_SEMAPHORE_LIMIT 1

ULONG_PTR
NTAPI
NnxUserGetCursorPosition(VOID)
{
    RECT rect;

    KeWaitForSingleObject(&MouseSemaphore, Executive, UserMode, FALSE, NULL);
    ULONG encodedPosition = MouseX | (MouseY << 16);
    int x, y;
    x = encodedPosition & 0xFFFF;
    y = encodedPosition >> 16;

    rect.top = y;
    rect.bottom = y + 10;
    rect.left = x;
    rect.right = x + 10;

    GdiFillRect(&rect, 0xFF000000 | encodedPosition);
    return encodedPosition;
}

static
VOID
MouseDpcRoutine(PKDPC Ddpc,
                PVOID DeferredContext,
                PVOID SystemArgument1,
                PVOID SystemArgument2)
{

    RECT rect;

    if (KeReadStateSemaphore(&MouseSemaphore) < MOUSE_SEMAPHORE_LIMIT)
    {
        KeReleaseSemaphore(&MouseSemaphore, 0, 1, FALSE);
    }

    rect.top = MouseY;
    rect.bottom = MouseY + 1;
    rect.left = MouseX;
    rect.right = MouseX + 1;

    //GdiFillRect(&rect, 0xFFFFFFFF);
}

static
VOID
DataReceived(UINT8 data[])
{
    INT16 dx, dy;

    MouseLeft = data[0] & 1;
    MouseMiddle = data[0] & 2;
    MouseRight = data[0] & 4;

    //dx = 0;
    dx = data[1];
    dx -= (data[0] << 4) & 0x100;
    dy = data[2];
    dy -= (data[0] << 3) & 0x100;

    MouseX += dx;
    MouseY -= dy;

    if (MouseY < 0)
    {
        MouseY = 0;
    }

    if (MouseX < 0)
    {
        MouseX = 0;
    }

    if (MouseY >= (INT32)gHeight)
    {
        MouseY = gHeight;
    }

    if (MouseX >= (INT32)gWidth)
    {
        MouseX = gWidth;
    }

    KeInsertQueueDpc(&MouseDpc, NULL, NULL);
}

static
BOOLEAN
MouseIsr(
    PKINTERRUPT InterruptObj,
    PVOID Ctx)
{
    static int i = 0;
    static UINT8 data[3];

    data[i++] = Ps2ReadNoPoll();
    if (i == 3)
    {
        DataReceived(data);
        i = 0;
    }

    return TRUE;
}

static
BOOLEAN
Ps2MouseInitPs2(UINT8 mouseDeviceId)
{
    if (mouseDeviceId != 0 && mouseDeviceId != 3 && mouseDeviceId != 4)
    {
        return FALSE;
    }

    Ps2SendToPort2(PS2_MOUSE_COMMAND_ENABLE_DATA_REPORTING);
    if (Ps2Read() != PS2_ACK)
    {
        return FALSE;
    }

    /* TODO */

    return TRUE;
}

NTSTATUS
Ps2MouseInitialize(VOID)
{
    PKINTERRUPT interrupt;
    UINT8 mouseDeviceId;

    mouseDeviceId = Ps2ResetPort2();
    if (mouseDeviceId == -1)
    {
        return STATUS_INTERNAL_ERROR;
    }

    if (Ps2MouseInitPs2(mouseDeviceId) == FALSE)
    {
        return STATUS_NO_SUCH_DEVICE;
    }

    KeInitializeDpc(&MouseDpc, MouseDpcRoutine, NULL);
    KeInitializeSemaphore(&MouseSemaphore, 1, MOUSE_SEMAPHORE_LIMIT);

    IoCreateInterrupt(
        &interrupt,
        MOUSE_VECTOR,
        IrqHandler,
        KeGetCurrentProcessorId(),
        3,
        FALSE,
        MouseIsr);

    /* FIXME */
    interrupt->IoApicVector = 2;
    interrupt->pfnSetMask = ApicSetInterruptMask;

    KeConnectInterrupt(interrupt);

    Ps2EnableInterruptPort2();

    return STATUS_SUCCESS;
}