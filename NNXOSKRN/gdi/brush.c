#include <gdi.h>

HBRUSH
NTAPI
GdiCreateBrushIndirect(
    const LOGBRUSH* logBrush)
{
    PGDI_BRUSH brush;
    HBRUSH hBrush;
    
    if (logBrush == NULL)
    {
        return NULL;
    }

    brush = (PGDI_BRUSH)GdiCreateObject(GDI_OBJECT_BRUSH_TYPE, sizeof(*brush));

    if (brush == NULL)
    {
        return NULL;
    }

    brush->lbColor = logBrush->lbColor;
    brush->lbHatch = logBrush->lbHatch;
    brush->lbStyle = logBrush->lbStyle;

    hBrush = GdiRegisterObject((PGDI_OBJECT_HEADER)brush);
    if (hBrush == NULL)
    {
        GdiFreeObject((PGDI_OBJECT_HEADER)brush);
    }

    return hBrush;
}

HBRUSH
NTAPI
GdiCreateSolidBrush(
    COLORREF color)
{
    LOGBRUSH indirect;
    indirect.lbColor = color;
    indirect.lbStyle = BS_SOLID;
    return GdiCreateBrushIndirect(&indirect);
}