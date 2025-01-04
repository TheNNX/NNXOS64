#include <gdi.h>

HDC 
NTAPI 
GdiCreateDC(
    const WCHAR* wszDriver, 
    const WCHAR* wszDevice, 
    PVOID devmode)
{
    GDI_HANDLE hdc;
    PGDI_DC dc = (PGDI_DC)GdiCreateObject(
        GDI_OBJECT_DC_TYPE,
        sizeof(*dc));
    if (dc == NULL)
    {
        return NULL;
    }

    dc->hBitmap = NULL;
    dc->hBrush = NULL;
    dc->hClippingRegion = NULL;
    dc->hFont = NULL;
    dc->hPalette = NULL;
    dc->hPath = NULL;
    dc->hPen = NULL;
    dc->hRegion = NULL;

    dc->hDevice = NULL;

    hdc = GdiRegisterObject((PGDI_OBJECT_HEADER)dc);
    if (hdc == NULL)
    {
        GdiFreeObject((PGDI_OBJECT_HEADER)dc);
    }

    return hdc;
}