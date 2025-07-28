#ifndef NNX_GDI_H
#define NNX_GDI_H

#ifndef __INTELLISENSE__ 
#ifdef __cplusplus
extern "C" {
#endif
#endif

#include <nnxtype.h>
#include <ntlist.h>

typedef ULONG_PTR GDI_HANDLE, *PGDI_HANDLE;
#define GDI_LOCKED_OBJECT ((PVOID)-1)

typedef GDI_HANDLE HDC,      *PHDC;
typedef GDI_HANDLE HBRUSH,   *PHBRUSH;
typedef GDI_HANDLE HBITMAP,  *PHBITMAP;
typedef GDI_HANDLE HPALETTE, *PHPALETTE;
typedef GDI_HANDLE HFONT,    *PHFONT;
typedef GDI_HANDLE HPATH,    *PHPATH;
typedef GDI_HANDLE HPEN,     *PHPEN;
typedef GDI_HANDLE HRGN,     *PHRGN;

typedef ULONG COLORREF, *PCOLORREF;

typedef enum _GDI_OBJECT_TYPE
{
    GDI_OBJECT_DC_TYPE,
    GDI_OBJECT_REGION_TYPE,
    GDI_OBJECT_BRUSH_TYPE,
    GDI_OBJECT_PALETTE_TYPE,
    GDI_OBJECT_FONT_TYPE,
    GDI_OBJECT_PATH_TYPE,
    GDI_OBJECT_PEN_TYPE,
    GDI_OBJECT_DEVICE_TYPE
} GDI_OBJECT_TYPE, *PGDI_OBJECT_TYPE;

typedef enum _GDI_BRUSH_STYLE
{
    BS_SOLID = 0,
    BS_NULL = 1,
    BS_HATCHED = 2,
    BS_PATTERN = 3,
    BS_INDEXED = 4,
    BS_DIBPATTERN = 5,
    BS_DIBPATTERNPT = 6,
    BS_PATTERN8X8 = 7,
    BS_DIBPATTERN8X8 = 8,
    BS_MONOPATTERN = 9
} GDI_BRUSH_STYLE, *PGDI_BRUSH_STYLE;

typedef struct _GDI_LOGBRUSH
{
    UINT lbStyle;
    COLORREF lbColor;
    ULONG_PTR lbHatch;
} LOGBRUSH, GDI_LOGBRUSH, *PLOGBRUSH, *PGDI_LOGBRUSH;

typedef struct _GDI_OBJECT_HEADER
{
    GDI_OBJECT_TYPE Type;
    GDI_HANDLE SelfHandle;
    VOID(*Destructor)(struct _GDI_OBJECT_HEADER* self);
    KSPIN_LOCK Lock;
} GDI_OBJECT_HEADER, *PGDI_OBJECT_HEADER;

typedef struct _GDI_BRUSH
{
    struct _GDI_OBJECT_HEADER;
    struct _GDI_LOGBRUSH;
} GDI_BRUSH, *PGDI_BRUSH;

typedef enum _GDI_REGION_MODE
{
    RGN_AND = 1,
    RGN_OR = 2,
    RGN_XOR = 3,
    RGN_DIFF = 4,
    RGN_COPY = 5
} GDI_REGION_MODE, *PGDI_REGION_MODE;

NTSTATUS 
NTAPI 
GdiInit(
    SIZE_T maxGdiObjects);

NTSTATUS
NTAPI
GdiStartTest(
    VOID);

PGDI_OBJECT_HEADER 
NTAPI 
GdiLockHandle(
    GDI_HANDLE handle);

NTSTATUS
NTAPI
GdiUnlockHandle(
    GDI_HANDLE handle,
    PVOID object);

GDI_HANDLE 
NTAPI 
GdiRegisterObject(
    PGDI_OBJECT_HEADER pObject);

VOID
NTAPI
GdiDestroy(
    GDI_HANDLE handle);

GDI_HANDLE 
NTAPI 
GdiCreateDevice(
    const WCHAR* name);

HDC 
NTAPI 
GdiCreateDC(
    const WCHAR* wszDriver, 
    const WCHAR* wszDevice, 
    PVOID devmode);

HBRUSH
NTAPI
GdiCreateBrushIndirect(
    const GDI_LOGBRUSH*);

GDI_HANDLE
NTAPI
GdiOpenDevice(
    PCWSTR wszName);

PGDI_OBJECT_HEADER
NTAPI
GdiCreateObject(
    GDI_OBJECT_TYPE type,
    SIZE_T size);

VOID
NTAPI
GdiMoveIntoHandle(
    GDI_HANDLE Dst,
    GDI_HANDLE Src);

VOID
NTAPI
GdiMovePtrIntoHandle(
    GDI_HANDLE Dst,
    PGDI_OBJECT_HEADER Src);

NTSYSAPI
VOID
NTAPI
GdiFreeObject(
    PGDI_OBJECT_HEADER pObject);

typedef struct _GDI_RECT
{
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} GDI_RECT, *PGDI_RECT;

typedef GDI_RECT RECT, *PRECT, *LPRECT;

typedef struct _GDI_DISPLAY_DEVICE
{
    struct _GDI_OBJECT_HEADER;
    WCHAR Name[32];
    LIST_ENTRY ListEntry;
} GDI_DISPLAY_DEVICE, *PGDI_DISPLAY_DEVICE;

typedef enum _GDI_REGION_TYPE
{
    ERROR = 0,
    NULLREGION = 1,
    SIMPLEREGION = 2,
    COMPLEXREGION = 3
} GDI_REGION_TYPE, *PGDI_REGION_TYPE;

typedef struct _GDI_REGION_HEADER
{
    struct _GDI_OBJECT_HEADER;
    GDI_REGION_TYPE RegionType;
} GDI_REGION_HEADER, *PGDI_REGION_HEADER;

typedef struct _GDI_REGION_RECTANGLE
{
    struct _GDI_REGION_HEADER;
    RECT Rect;
} GDI_REGION_RECTANGLE, *PGDI_REGION_RECTANGLE;

typedef struct _GDI_REGION_ARBITRARY
{
    struct _GDI_REGION_HEADER;
    RECT BoundingBox;
    PBYTE Data;
} GDI_REGION_ARBITRARY, *PGDI_REGION_ARBITRARY;

typedef struct _GDI_DC
{
    HBRUSH   hBrush;
    HBITMAP  hBitmap;
    HPALETTE hPalette;
    HFONT    hFont;
    HPATH    hPath;
    HPEN     hPen;
    HRGN  hRegion;

    int iBackgroundMode;
    int iDrawingMode;
    int iMappingMode;
    int iPolygonFillMode;
    int iStretchingMode;

    GDI_HANDLE hClippingRegion;

    GDI_HANDLE hDevice;
} GDI_DC, *PGDI_DC;

int
NTAPI
GdiCombineRgn(
    HRGN hRgnDst,
    HRGN hRgnSrc1,
    HRGN hRgnSrc2,
    int iMode);

HRGN
NTAPI
GdiCreateRectRgn(
    const RECT* rect);

HRGN
NTAPI
GdiCreateEmptyRgn(
    VOID);

NTSYSAPI
VOID
NTAPI
GdiFillRect(LPRECT rect,
            COLORREF colour);

#ifndef __INTELLISENSE__ 
#ifdef __cplusplus
}
#endif
#endif

#endif