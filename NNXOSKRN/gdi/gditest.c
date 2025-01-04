#include <gdi.h>
#include <SimpleTextIO.h>

VOID
GdiDebugFillRegion(
    HRGN hRgn,
    COLORREF c);

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

    PrintT("Starting " __FUNCTION__ ", current IRQL %X\n\n", KeGetCurrentIrql());

    PrintT("Creating rect region 1\n");
    rect1.left = 100;
    rect1.right = 200;
    rect1.top = 200;
    rect1.bottom = 300;
    rgn1 = GdiCreateRectRgn(&rect1);
    PrintT("Result: %X\n\n", rgn1);
    GdiDebugFillRegion(rgn1, 0x7F0000);

    PrintT("Creating rect region 2\n");
    rect2.left = 150;
    rect2.right = 300;
    rect2.top = 120;
    rect2.bottom = 290;
    rgn2 = GdiCreateRectRgn(&rect2);
    PrintT("Result: %X\n\n", rgn2);
    GdiDebugFillRegion(rgn2, 0x7F00FF);

    PrintT("Creating empty result region\n");
    rgn3 = GdiCreateEmptyRgn();
    PrintT("Result: %X\n\n", rgn3);

    PrintT("Doing region intersection\n");
    res = GdiCombineRgn(rgn3, rgn1, rgn2, RGN_AND);
    PrintT("Result: %i\n\n", res);
    GdiDebugFillRegion(rgn3, 0x00FFFF);

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
    GdiDebugFillRegion(rgn1, 0x000000);

    PrintT("Doing region diff\n");
    res = GdiCombineRgn(rgn2, rgn1, rgn3, RGN_DIFF);
    PrintT("Result: %i\n\n", res);
    GdiDebugFillRegion(rgn2, 0xFFFFFF);

    PrintT(""__FUNCTION__ " done. Returning to usermode application.\n");
    return STATUS_SUCCESS;
}