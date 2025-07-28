#include <scheduler.h>
#include <object.h>
#include <bugcheck.h>
#include <mm.h>
#include <ntqueue.h>
#include <pool.h>
#include <gdi.h>

PKQUEUE dummy;

NTSYSAPI
ULONG_PTR 
NTAPI
KeTestSyscall1(
    ULONG_PTR p1,
    ULONG_PTR p2,
    ULONG_PTR p3,
    ULONG_PTR p4,
    ULONG_PTR p5,
    ULONG_PTR p6,
    ULONG_PTR p7,
    ULONG_PTR p8);

NTSYSAPI
ULONG_PTR
NTAPI
KeTestSyscall2(
    ULONG_PTR p1,
    ULONG_PTR p2,
    ULONG_PTR p3,
    ULONG_PTR p4,
    ULONG_PTR p5,
    ULONG_PTR p6,
    ULONG_PTR p7,
    ULONG_PTR p8,
    ULONG_PTR p9);

NTSYSAPI
ULONG_PTR
NTAPI
NnxUserGetCursorPosition(VOID);

VOID CursorThreadFunc()
{
    while (1)
    {
        RECT rect;
        ULONG_PTR encodedPosition = NnxUserGetCursorPosition();

        //KeTestSyscall1(1, 2, 3, 4, 5, 6, 7, 8);

        int x, y;
        x = encodedPosition & 0xFFFF;
        y = encodedPosition >> 16;

        rect.top = y;
        rect.bottom = y + 1;
        rect.left = x;
        rect.right = x + 1;

        //GdiFillRect(&rect, 0xFF000000 | encodedPosition);
    }
}

NTSTATUS
NTAPI
LdrInitThread()
{
    NTSTATUS status;
    CONTEXT Context;
    HANDLE hCursorThread;

    dummy = ExAllocatePoolWithTag(NonPagedPool, sizeof(*dummy), 'QQQQ');
    KeInitializeQueue(dummy, 0);

    status = GdiInit(4096 * 4);
    if (!NT_SUCCESS(status))
    {
        while (1);
        return status;
    }

    status = GdiStartTest();
    if (!NT_SUCCESS(status))
    {
        while (1);
        return status;
    }

    Context.Rip = CursorThreadFunc;
    NtCreateThread(&hCursorThread, UserMode, NULL, NULL, NULL, &Context, NULL, FALSE);

    while (1)
    {
        ULONG_PTR result = KeTestSyscall1(1, 2, 3, 4, 5, 6, 7, 8);
        ULONG64 timeout = -10000000;
        PVOID ptr[] = { dummy };
        KeWaitForMultipleObjects(1, ptr, WaitAny, Executive, UserMode, TRUE, &timeout, NULL);
        
        //result = KeTestSyscall1(result, 2, 3, 4, 5, 6, 7, 8);
        result = KeTestSyscall2(1, 2, 3, 4, 5, result, 7, 8, 9);
    }
    while (1);
}