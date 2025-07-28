#include <gdi.h>
#include <pool.h>
#include <ntdebug.h>
#include <SimpleTextIO.h>

PGDI_REGION_HEADER
NTAPI
GdiCreateEmptyRgnPtr(VOID)
{
    PGDI_REGION_HEADER region;

    region = (PGDI_REGION_HEADER)
        GdiCreateObject(
            GDI_OBJECT_REGION_TYPE,
            sizeof(*region));
    if (region == NULL)
    {
        return NULL;
    }

    region->RegionType = NULLREGION;
    return region;
}

HRGN
NTAPI
GdiCreateEmptyRgn(VOID)
{
    HRGN hRegion;
    PGDI_OBJECT_HEADER region = (PGDI_OBJECT_HEADER)GdiCreateEmptyRgnPtr();

    if (region == NULL)
    {
        return NULL;
    }

    hRegion = GdiRegisterObject(region);
    if (hRegion == NULL)
    {
        GdiFreeObject(region);
    }

    return hRegion;
}

PGDI_REGION_RECTANGLE 
NTAPI
GdiCreateRectRgnPtr(
    const RECT* rect)
{
    PGDI_REGION_RECTANGLE region;

    if (rect == NULL)
    {
        PrintT("["__FILE__":%d] rect is NULL\n", __LINE__);
        return NULL;
    }

    region = (PGDI_REGION_RECTANGLE)
        GdiCreateObject(
            GDI_OBJECT_REGION_TYPE,
            sizeof(*region));
    if (region == NULL)
    {
        PrintT("["__FILE__":%d] GdiCreateObject failed\n", __LINE__);
        return NULL;
    }

    return region;
}

HRGN
NTAPI
GdiCreateRectRgn(
    const RECT* rect)
{
    GDI_HANDLE hRegion;
    PGDI_REGION_RECTANGLE region;
    
    region = GdiCreateRectRgnPtr(rect);
    if (region == NULL)
    {
        return NULL;
    }

    hRegion = GdiRegisterObject((PGDI_OBJECT_HEADER)region);
    if (hRegion == NULL)
    {
        PrintT("["__FILE__":%d] GdiRegisterObject failed\n", __LINE__);
        GdiFreeObject((PGDI_OBJECT_HEADER)region);
        return NULL;
    }

    region->Rect = *rect;
    region->RegionType = SIMPLEREGION;

    return hRegion;
}

static
VOID
GdiComplexRegionDestructor(
    PGDI_REGION_ARBITRARY self)
{
    ASSERTMSG(
        self && 
        self->Type == GDI_OBJECT_REGION_TYPE && 
        self->RegionType == COMPLEXREGION, 
        "Complex region destructor called on a non-complex region");

    if (self->Data != NULL)
    {
        ExFreePoolWithTag(self->Data, 'RGNC');
    }
}

static
SIZE_T
ComplexRegionDataSizeForRect(
    const RECT* rect)
{
    return ((rect->bottom - rect->top) * (rect->right - rect->left) + 7) / 8;
}

PGDI_REGION_ARBITRARY
NTAPI
GdiCreateComplexRgnPtr(
    const RECT* rect)
{
    PGDI_REGION_ARBITRARY region;
    SIZE_T dataSize;

    if (rect == NULL || rect->top >= rect->bottom || rect->left >= rect->right)
    {
        return NULL;
    }

    region = (PGDI_REGION_ARBITRARY)
        GdiCreateObject(
            GDI_OBJECT_REGION_TYPE,
            sizeof(*region));
    if (region == NULL)
    {
        return NULL;
    }

    /* Number of bits for a 1-bit bitmap of the region. */
    dataSize = ComplexRegionDataSizeForRect(rect);

    region->BoundingBox = *rect;
    region->RegionType = COMPLEXREGION;
    region->Data = ExAllocatePoolWithTag(PagedPool, dataSize, 'RGNC');
    if (region->Data == NULL)
    {
        GdiFreeObject((PGDI_OBJECT_HEADER)region);
        return NULL;
    }
    region->Destructor = (PVOID)GdiComplexRegionDestructor;
    return region;
}

HRGN
NTAPI
GdiCreateComplexRgn(
    const RECT* rect)
{
    HRGN hRegion;
    PGDI_OBJECT_HEADER region =
        (PGDI_OBJECT_HEADER)GdiCreateComplexRgnPtr(rect);

    hRegion = GdiRegisterObject(region);
    if (hRegion == NULL)
    {
        GdiFreeObject(region);
        return NULL;
    }

    return hRegion;
}

static LONG Clamp(LONG min, LONG val, LONG max)
{
    if (val < min)
        return min;

    if (val > max)
        return max;

    return val;
}

static void RectAnd(RECT* result, const RECT* r1, const RECT* r2)
{
    result->left = Clamp(r1->left, r2->left, r1->right);
    result->right = Clamp(r1->left, r2->right, r1->right);
    result->top = Clamp(r1->top, r2->top, r1->bottom);
    result->bottom = Clamp(r1->top, r2->bottom, r1->bottom);
}

static int RegionSimpleAndSimple(
    HRGN dst,
    PGDI_REGION_RECTANGLE src1,
    PGDI_REGION_RECTANGLE src2)
{
    HRGN hTmp;
    RECT result;

    RectAnd(&result, &src1->Rect, &src2->Rect);

    if (result.right - result.left <= 0 || result.bottom - result.top <= 0)
    {
        GdiMoveIntoHandle(dst, GdiCreateEmptyRgn());
        return NULLREGION;
    }

    hTmp = GdiCreateRectRgn(&result);
    if (hTmp == NULL)
    {
        return ERROR;
    }
    GdiMoveIntoHandle(dst, hTmp);
    return SIMPLEREGION;
}

static int RegionComplexAndRectPtr(
    PGDI_REGION_ARBITRARY src1,
    const RECT* src2Rect,
    PGDI_REGION_HEADER* pOutResult)
{
    RECT intersection;
    PGDI_REGION_ARBITRARY pTmpComplex;
    LONG x, y;
    BOOLEAN empty;

    /* Get the intersection of the complex region bounding box
       and the simple region. */
    RectAnd(&intersection, &src1->BoundingBox, src2Rect);
    if (intersection.right - intersection.left <= 0 || intersection.bottom - intersection.top <= 0)
    {
        *pOutResult = GdiCreateEmptyRgnPtr();
        return NULLREGION;
    }

    pTmpComplex = GdiCreateComplexRgnPtr(&intersection);
    if (pTmpComplex == NULL)
    {
        return ERROR;
    }

    empty = TRUE;

    for (y = intersection.top; y < intersection.bottom; y++)
    {
        ULONG nWidth = (src2Rect->right - src2Rect->left);
        ULONG ny = (y - src2Rect->top);

        ULONG oWidth = (src1->BoundingBox.right - src1->BoundingBox.left);
        ULONG oy = (y - src1->BoundingBox.top);

        SIZE_T nPremul = ny * nWidth;
        SIZE_T oPremul = oy * oWidth;

        for (x = intersection.left; x < intersection.right; x++)
        {
            ULONG nx = (x - src2Rect->left);
            ULONG ox = (x - src1->BoundingBox.left);

            SIZE_T nBitIndex = nPremul + nx;
            SIZE_T nByteIndex = nBitIndex >> 3;

            SIZE_T oBitIndex = oPremul + ox;
            SIZE_T oByteIndex = oBitIndex >> 3;

            SIZE_T nBitOffset = nBitIndex & 0b111;
            SIZE_T oBitOffset = oBitIndex & 0b111;

            BOOLEAN set = (src1->Data[oByteIndex] & (1 << oBitOffset)) != 0;
            pTmpComplex->Data[nByteIndex] &= ~(!set << nBitOffset);

            if (pTmpComplex->Data[nBitIndex] != 0)
            {
                empty = FALSE;
            }
        }
    }

    if (empty)
    {
        GdiFreeObject((PGDI_OBJECT_HEADER)pTmpComplex);
        *pOutResult = GdiCreateEmptyRgnPtr();
        return NULLREGION;
    }
    else
    {
        *pOutResult = (PGDI_REGION_HEADER)pTmpComplex;
        return COMPLEXREGION;
    }
}

static int RegionComplexAndSimple(
    HRGN Dst,
    PGDI_REGION_ARBITRARY src1,
    PGDI_REGION_RECTANGLE src2)
{
    PGDI_REGION_HEADER region;
    int result;
    
    region = NULL;

    result = RegionComplexAndRectPtr(src1, &src2->Rect, &region);
    if (result == ERROR)
    {
        return ERROR;
    }

    GdiMovePtrIntoHandle(Dst, (PGDI_OBJECT_HEADER)region);
    return result;
}

static int RegionComplexAndComplex(
    HRGN dst,
    PGDI_REGION_ARBITRARY src1,
    PGDI_REGION_ARBITRARY src2)
{
    int result1, result2;
    PGDI_REGION_HEADER reg1, reg2;

    result1 = RegionComplexAndRectPtr(src1, &src2->BoundingBox, &reg1);
    if (result1 == ERROR)
    {
        return ERROR;
    }

    result2 = RegionComplexAndRectPtr(src2, &src1->BoundingBox, &reg2);
    if (result2 == ERROR)
    {
        GdiFreeObject((PGDI_OBJECT_HEADER)reg1);
        return ERROR;
    }

    if (result2 == NULLREGION || result1 == NULLREGION)
    {
        GdiFreeObject((PGDI_OBJECT_HEADER)reg1);
        GdiFreeObject((PGDI_OBJECT_HEADER)reg2);
        GdiMovePtrIntoHandle(dst, (PGDI_OBJECT_HEADER)GdiCreateEmptyRgnPtr());
        return NULLREGION;
    }

    if (result2 == COMPLEXREGION && result1 == COMPLEXREGION)
    {
        PGDI_REGION_ARBITRARY complex1 = (PGDI_REGION_ARBITRARY)reg1;
        PGDI_REGION_ARBITRARY complex2 = (PGDI_REGION_ARBITRARY)reg2;
        SIZE_T s, i;
        BOOLEAN empty = TRUE;

        if (complex1->BoundingBox.left != complex2->BoundingBox.left ||
            complex1->BoundingBox.right != complex2->BoundingBox.right ||
            complex1->BoundingBox.top != complex2->BoundingBox.top ||
            complex1->BoundingBox.bottom != complex2->BoundingBox.bottom)
        {
            PrintT("[GDI] Warning: complex bbox intersection mismatch in "__FUNCTION__" at "__FILE__":%i\n", __LINE__);
            GdiFreeObject((PGDI_OBJECT_HEADER)reg2);
            GdiFreeObject((PGDI_OBJECT_HEADER)reg1);
            return ERROR;
        }

        s = ComplexRegionDataSizeForRect(&complex1->BoundingBox);
        if (s != ComplexRegionDataSizeForRect(&complex2->BoundingBox))
        {
            PrintT("[GDI] Warning: complex bbox data size mismatch. How did the previous test pass???\n");
            GdiFreeObject((PGDI_OBJECT_HEADER)reg2);
            GdiFreeObject((PGDI_OBJECT_HEADER)reg1);
            return ERROR;
        }

        for (i = 0; i < s; i++)
        {
            complex1->Data[i] = complex1->Data[i] & complex2->Data[i];
        }

        GdiFreeObject((PGDI_OBJECT_HEADER)reg2);
        if (empty)
        {
            GdiFreeObject((PGDI_OBJECT_HEADER)reg1);
            GdiMovePtrIntoHandle(dst, (PGDI_OBJECT_HEADER)GdiCreateEmptyRgnPtr());
            return NULLREGION;
        }
        else
        {
            GdiMovePtrIntoHandle(dst, (PGDI_OBJECT_HEADER)reg1);
            return COMPLEXREGION;
        }
    }
    else
    {
        ASSERT(FALSE);
        return ERROR;
    }
}

static int RegionAnd(
    HRGN dst, 
    PGDI_REGION_HEADER src1,
    PGDI_REGION_HEADER src2)
{
    if (src1->RegionType == NULLREGION || src2->RegionType == NULLREGION)
    {
        GdiMovePtrIntoHandle(dst, (PGDI_OBJECT_HEADER)GdiCreateEmptyRgnPtr());
        return NULLREGION;
    }

    if (src2->RegionType > src1->RegionType)
    {
        PGDI_REGION_HEADER swapTmp = src1;
        src1 = src2;
        src2 = swapTmp;
    }
    
    if (src1->RegionType == SIMPLEREGION && src2->RegionType == SIMPLEREGION)
    {
        return RegionSimpleAndSimple(
            dst, 
            (PGDI_REGION_RECTANGLE)src1, 
            (PGDI_REGION_RECTANGLE)src2);
    }
    
    /* TODO: Test */
    if (src1->RegionType == COMPLEXREGION && src2->RegionType == SIMPLEREGION)
    {
        return RegionComplexAndSimple(
            dst,
            (PGDI_REGION_ARBITRARY)src1,
            (PGDI_REGION_RECTANGLE)src2);
    }

    /* TODO: Test */
    if (src1->RegionType == COMPLEXREGION && src2->RegionType == COMPLEXREGION)
    {
        return RegionComplexAndComplex(
            dst,
            (PGDI_REGION_ARBITRARY)src1,
            (PGDI_REGION_ARBITRARY)src2);
    }

    /* TODO: not implemented. */
    PrintT("["__FILE__":%d] Warn - Not implemented\n", __LINE__);
    return ERROR;
}

static int RegionComplexCopyDiff(
    HRGN dst,
    PGDI_REGION_HEADER src1,
    PGDI_REGION_HEADER src2)
{
    SIZE_T complexDataSize, i;
    PGDI_REGION_ARBITRARY complexCopy;
    LONG x, y;
    BOOLEAN empty;
    const RECT* bbox1;
    const RECT* bbox2;
    RECT intersection;

    PVOID pDstObj;

    ASSERT(src1->RegionType == SIMPLEREGION || src1->RegionType == COMPLEXREGION);

    if (src1->RegionType == SIMPLEREGION)
    {
        complexCopy = GdiCreateComplexRgnPtr(&((PGDI_REGION_RECTANGLE)src1)->Rect);
    }
    else
    {
        complexCopy = GdiCreateComplexRgnPtr(&((PGDI_REGION_ARBITRARY)src1)->BoundingBox);
    }

    if (complexCopy == NULL)
    {
        return ERROR;
    }

    pDstObj = GdiLockHandle(dst);
    GdiFreeObject(pDstObj);

    complexCopy->SelfHandle = dst;

    complexDataSize = ComplexRegionDataSizeForRect(&complexCopy->BoundingBox);
    for (i = 0; i < complexDataSize; i++)
    {
        if (src1->RegionType == SIMPLEREGION)
        {
            complexCopy->Data[i] = 0xFF;
        }
        else
        {
            complexCopy->Data[i] = ((PGDI_REGION_ARBITRARY)src1)->Data[i];
        }
    }
    
    bbox1 = &complexCopy->BoundingBox;
   
    ASSERT(src2->RegionType == COMPLEXREGION || src2->RegionType == SIMPLEREGION);
    if (src2->RegionType == COMPLEXREGION)
    {
        bbox2 = &((PGDI_REGION_ARBITRARY)src2)->BoundingBox;
    }
    else
    {
        bbox2 = &((PGDI_REGION_RECTANGLE)src2)->Rect;
    }

    /* Get the intersection of the complex region bounding box
       and the simple region. */
    RectAnd(&intersection, bbox1, bbox2);
    empty = FALSE;
    
    for (y = intersection.top; y < intersection.bottom; y++)
    {
        ULONG nWidth = (bbox1->right - bbox1->left);
        ULONG ny = (y - bbox1->top);

        ULONG oWidth = (bbox2->right - bbox2->left);
        ULONG oy = (y - bbox2->top);

        SIZE_T nPremul = ny * nWidth;
        SIZE_T oPremul = oy * oWidth;

        for (x = intersection.left; x < intersection.right; x++)
        {
            ULONG nx = (x - bbox1->left);
            ULONG ox = (x - bbox2->left);

            SIZE_T nBitIndex = nPremul + nx;
            SIZE_T nByteIndex = nBitIndex >> 3;

            SIZE_T oBitIndex = oPremul + ox;
            SIZE_T oByteIndex = oBitIndex >> 3;

            SIZE_T nBitOffset = nBitIndex & 0b111;
            SIZE_T oBitOffset = oBitIndex & 0b111;

            BOOLEAN set;

            if (src2->RegionType == COMPLEXREGION)
            {
                PGDI_REGION_ARBITRARY pSrc2Complex = (PVOID)src2;
                
                set = (pSrc2Complex->Data[oByteIndex] & (1 << oBitOffset)) != 0;
            }
            else
            {
                set = TRUE;
            }

            complexCopy->Data[nByteIndex] &= ~(set << nBitOffset);

            if (complexCopy->Data[nBitIndex] != 0)
            {
                empty = FALSE;
            }
        }
    }
    
    if (empty == FALSE)
    {
        GdiUnlockHandle(dst, complexCopy);
        return COMPLEXREGION;
    }
    
    GdiFreeObject((PVOID)complexCopy);
    GdiUnlockHandle(dst, GdiCreateEmptyRgnPtr());
    return NULLREGION;
}

static int RegionCopy(
    HRGN dst,
    PGDI_REGION_HEADER src)
{
    size_t complexDataSize, i;
    PVOID pDstObj = GdiLockHandle(dst);
    PVOID pNewObj = NULL;

    PGDI_REGION_ARBITRARY pSrcComplex, pNewComplex;

    switch (src->RegionType)
    {
    case NULLREGION:
        pNewObj = GdiCreateEmptyRgnPtr();
        break;
    case SIMPLEREGION:
        pNewObj = GdiCreateRectRgnPtr(&((PGDI_REGION_RECTANGLE)src)->Rect);
        break;
    case COMPLEXREGION:
        pSrcComplex = (PGDI_REGION_ARBITRARY)src;
        pNewObj = GdiCreateComplexRgnPtr(&pSrcComplex->BoundingBox);
        pNewComplex = (PGDI_REGION_ARBITRARY) pNewObj;

        complexDataSize = ComplexRegionDataSizeForRect(&pSrcComplex->BoundingBox);
        for (i = 0; i < complexDataSize; i++)
        {
            pNewComplex->Data[i] = pSrcComplex->Data[i];
        }
        
        break;
    }

    if (pNewObj != NULL)
    {
        GdiFreeObject(pDstObj);
        return src->RegionType;
    }

    GdiUnlockHandle(dst, pDstObj);
    return ERROR;
}

static int RegionDiff(
    HRGN dst,
    PGDI_REGION_HEADER src1,
    PGDI_REGION_HEADER src2)
{
    if (src2->RegionType == ERROR || src1->RegionType == ERROR)
    {
        return ERROR;
    }

    if (src1->RegionType == NULLREGION)
    {
        return RegionCopy(dst, src2);
    }
    else if (src2->RegionType == NULLREGION)
    {
        return RegionCopy(dst, src1);
    }
    else if (src1->RegionType == SIMPLEREGION || 
             src1->RegionType == COMPLEXREGION)
    {
        return RegionComplexCopyDiff(dst, src1, src2);
    }

    return ERROR;
}

int
NTAPI
GdiCombineRgn(
    HRGN hRgnDst, 
    HRGN hRgnSrc1, 
    HRGN hRgnSrc2, 
    int iMode)
{
    int result;
    PGDI_REGION_HEADER src1, src2;

    if (hRgnDst == NULL)
    {
        return ERROR;
    }

    /* These cases have to be handled separately. */
    if (hRgnSrc1 == hRgnSrc2 || hRgnSrc1 == hRgnDst || hRgnSrc2 == hRgnDst)
    {
        /* TODO */
        ASSERT(hRgnSrc1 != hRgnSrc2);
        ASSERT(hRgnSrc1 != hRgnDst);
        ASSERT(hRgnSrc2 != hRgnDst);
    }

    src1 = (PGDI_REGION_HEADER)GdiLockHandle(hRgnSrc1);
    src2 = (PGDI_REGION_HEADER)GdiLockHandle(hRgnSrc2);

    if (src1 == NULL || src2 == NULL)
    {
        GdiUnlockHandle(hRgnSrc1, src1);
        GdiUnlockHandle(hRgnSrc2, src2);

        return ERROR;
    }

    result = ERROR;

    switch (iMode)
    {
    case RGN_AND:
        result = RegionAnd(hRgnDst, src1, src2);
        break;
    case RGN_COPY:
        /* TODO: not implemented */
        break;
    case RGN_DIFF:
        result = RegionDiff(hRgnDst, src1, src2);
        break;
    case RGN_OR:
        /* TODO: not implemented */
        break;
    case RGN_XOR:
        /* TODO: not implemented */
        break;
    }

    GdiUnlockHandle(hRgnSrc1, src1);
    GdiUnlockHandle(hRgnSrc2, src2);
    
    return result;
}

VOID 
DebugFillRect(
    const RECT* rect,
    ULONG_PTR color,
    COLORREF(*colorFunction)(LONG x, LONG y, ULONG_PTR))
{
    LONG x, y;
    volatile UINT32* line = gFramebuffer + rect->top * gPixelsPerScanline;

    for (y = rect->top; y < rect->bottom; y++)
    {
        for (x = rect->left; x < rect->right; x++)
        {
            if (colorFunction(x, y, color) & 0xFF000000)
            {
                if (x >= 0 && y >= 0 && (UINT32)y <= gHeight && (UINT32)x <= gWidth)
                line[x] = colorFunction(x, y, color);
            }
        }

        line += gPixelsPerScanline;
    }
}

VOID 
GdiDebugFillSimpleRegion(
    PGDI_REGION_RECTANGLE region,
    ULONG_PTR color,
    COLORREF(*colorFunction)(LONG x, LONG y, ULONG_PTR))
{
    DebugFillRect(&region->Rect, color, colorFunction);
}

VOID 
GdiDebugFillComplexRegion(
    PGDI_REGION_ARBITRARY region,
    ULONG_PTR color,
    COLORREF(*colorFunction)(LONG x, LONG y, ULONG_PTR))
{
    const RECT* rect = &region->BoundingBox;
    LONG x, y;
    volatile UINT32* line = gFramebuffer + rect->top * gPixelsPerScanline;

    for (y = rect->top; y < rect->bottom; y++)
    {
        ULONG nWidth = (rect->right - rect->left);
        ULONG ny = (y - rect->top);
        SIZE_T nPremul = ny * nWidth;

        for (x = rect->left; x < rect->right; x++)
        {
            ULONG nx = (x - rect->left);
            SIZE_T nBitIndex = nPremul + nx;
            SIZE_T nByteIndex = nBitIndex >> 3;

            SIZE_T nBitOffset = nBitIndex & 0b111;

            BOOLEAN set = (region->Data[nByteIndex] & (1 << nBitOffset)) != 0;

            if (set)
            {
                if (colorFunction(x, y, color) & 0xFF000000)
                {
                    line[x] = colorFunction(x, y, color);
                }
            }
        }

        line += gPixelsPerScanline;
    }
}

VOID
GdiDebugFillRegion(
    HRGN hRgn,
    ULONG_PTR colorFunctionArg,
    COLORREF(*colorFunction)(LONG x, LONG y, ULONG_PTR))
{
    PGDI_REGION_HEADER region = (PGDI_REGION_HEADER)GdiLockHandle(hRgn);

    switch (region->RegionType)
    {
    case SIMPLEREGION:
        GdiDebugFillSimpleRegion((PGDI_REGION_RECTANGLE)region, colorFunctionArg, colorFunction);
        break;
    case COMPLEXREGION:
        GdiDebugFillComplexRegion((PGDI_REGION_ARBITRARY)region, colorFunctionArg, colorFunction);
        break;
    case NULLREGION:
        break;
    case ERROR:
        PrintT("Warning - trying to fill an error region\n");
        break;
    }

    GdiUnlockHandle(hRgn, region);
}