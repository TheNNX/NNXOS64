#include <gdi.h>
#include <SimpleTextIO.h>

static 
COLORREF 
SolidFill(
    LONG x, LONG y, ULONG_PTR colorFunctionArg)
{
    return (COLORREF)colorFunctionArg;
}

static 
COLORREF
CheckerboardFill(
    LONG x, LONG y, ULONG_PTR colorFunctionArg)
{
    if ((x + y) % 2 == 0)
    {
        return (COLORREF)colorFunctionArg;
    }
    return 0;
}

VOID
GdiDebugFillRegion(
    HRGN hRgn,
    ULONG_PTR colorFunctionArg,
    COLORREF(*colorFunction)(LONG x, LONG y, ULONG_PTR));

NTSTATUS
NTAPI
GdiStartTest()
{
    HRGN rgn1, rgn2, rgn3;
    RECT rect1, rect2;
    int res;

    UINT32 bbox[4];
    TextIoGetBoundingBox(bbox);

    bbox[2] = bbox[3] / 2;
    TextIoSetBoundingBox(bbox);
    rect1.bottom = bbox[2] - 1;
    rect1.top = 0;
    rect1.left = bbox[0];
    rect1.right = bbox[1];
    rgn1 = GdiCreateRectRgn(&rect1);

    GdiDebugFillRegion(rgn1, 0xFF3F3F3F, SolidFill);
    GdiDestroy(rgn1);

    PrintT("Starting " __FUNCTION__ ", current IRQL %X\n\n", KeGetCurrentIrql());

    PrintT("Creating rect region 1\n");
    rect1.left = 100;
    rect1.right = 200;
    rect1.top = 200;
    rect1.bottom = 300;
    rgn1 = GdiCreateRectRgn(&rect1);
    PrintT("Result: %X\n\n", rgn1);
    GdiDebugFillRegion(rgn1, 0xFF00007F, SolidFill);

    PrintT("Creating rect region 2\n");
    rect2.left = 150;
    rect2.right = 300;
    rect2.top = 120;
    rect2.bottom = 290;
    rgn2 = GdiCreateRectRgn(&rect2);
    PrintT("Result: %X\n\n", rgn2);
    GdiDebugFillRegion(rgn2, 0xFF7F7F00, SolidFill);

    PrintT("Creating empty result region\n");
    rgn3 = GdiCreateEmptyRgn();
    PrintT("Result: %X\n\n", rgn3);

    PrintT("Doing region diff\n");
    res = GdiCombineRgn(rgn3, rgn1, rgn2, RGN_DIFF);
    PrintT("Result: %i\n\n", res);
    GdiDebugFillRegion(rgn3, 0xFF007F00, CheckerboardFill);

    PrintT("Destroying region 1\n");
    GdiDestroy(rgn1);
    PrintT("\n\n");

    PrintT("Creating new region 1\n");
    rect1.left = 16;
    rect1.right = 400;
    rect1.top = 210;
    rect1.bottom = 220;
    rgn1 = GdiCreateRectRgn(&rect1);
    PrintT("Result: %i\n\n", rgn1);
    //GdiDebugFillRegion(rgn1, 0x000000);

    PrintT("Doing region diff\n");
    res = GdiCombineRgn(rgn2, rgn1, rgn3, RGN_DIFF);
    PrintT("Result: %i\n\n", res);
    GdiDebugFillRegion(rgn2, 0xFFFFFFFF, SolidFill);

    PrintT(""__FUNCTION__ " done. Returning to usermode application.\n");
    return STATUS_SUCCESS;
}

VOID
DebugFillRect(const RECT* rect,
              ULONG_PTR colour,
              COLORREF(*colorFunction)(LONG x, LONG y, ULONG_PTR));

#include <scheduler.h>

NTSYSAPI
VOID
NTAPI
GdiFillRect(LPRECT rect,
            COLORREF colour)
{
    //PrintT("%X (%i,%i) x (%i,%i)    =     %X\n", rect, 
    //       (UINT64)rect->left, (UINT64)rect->top, (UINT64)rect->right, (UINT64)rect->bottom, (UINT64)colour);
    DebugFillRect(rect, colour, SolidFill);
}