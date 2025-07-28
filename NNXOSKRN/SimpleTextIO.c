#include <SimpleTextIo.h>
#include <physical_allocator.h>
#include <rtl.h>
#include <text.h>

#define UNKNOWNMARK {0x46,0xBB,0xFB,0xF7,0xEF,0xEF,0xFF,0x6E}

UINT64 FrameBufferSize()
{
    return (((UINT64) gFramebufferEnd) - ((UINT64) gFramebuffer));
}

UINT32 TextIoDeltaX = 0;
UINT32 TextIoDeltaY = 0;

BOOL IsCurrentOperationGlobal;

static UINT8 align = 0;

UINT32 gCursorX = 0, gCursorY = 0, gColor = 0, gBackdrop = 0;
UINT8 gRenderBackdrop = 0;

UINT32 gMinX = 0, gMaxX = 0;
UINT32 gMinY = 0, gMaxY = 0;

static UINT8 initialized = 0;

static UINT8 StaticFont8x8[][8] =
{
    /* Non-printable characters. */
    UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, 
    UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, 
    UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, 
    UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, 
    UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, 
    UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, UNKNOWNMARK, 
    UNKNOWNMARK, UNKNOWNMARK,

    /* Symbols. */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /*   */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x10}, /* ! */
    {0x28, 0x28, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00}, /* " */ 
    {0x28, 0x28, 0x7C, 0x28, 0x28, 0x7C, 0x28, 0x28}, /* # */
    {0x38, 0x54, 0x50, 0x38, 0x14, 0x14, 0x54, 0x38}, /* $ */ 
    {0x44, 0x08, 0x08, 0x10, 0x20, 0x20, 0x44, 0x00}, /* % */ 
    {0x38, 0x44, 0x44, 0x38, 0x44, 0x46, 0x44, 0x3A}, /* & */ 
    {0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ' */
    {0x08, 0x10, 0x20, 0x20, 0x20, 0x20, 0x10, 0x08}, /* ( */
    {0x20, 0x10, 0x08, 0x08, 0x08, 0x08, 0x10, 0x20}, /* ) */ 
    {0x10, 0x7C, 0x38, 0x6c, 0x00, 0x00, 0x00, 0x00}, /* * */ 
    {0x00, 0x00, 0x10, 0x10, 0x7C, 0x10, 0x10, 0x00}, /* + */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10}, /* , */ 
    {0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00}, /* - */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10}, /* . */
    {0x04, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x40}, /* / */

    /* Digits. */
    {0x38, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38}, /* 0 */
    {0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38}, /* 1 */
    {0x38, 0x44, 0x04, 0x04, 0x08, 0x10, 0x20, 0x7C}, /* 2 */
    {0x38, 0x04, 0x04, 0x38, 0x04, 0x04, 0x04, 0x38}, /* 3 */
    {0x10, 0x30, 0x50, 0x90, 0xF8, 0x10, 0x10, 0x10}, /* 4 */
    {0x7C, 0x40, 0x40, 0x78, 0x04, 0x04, 0x04, 0x78}, /* 5 */
    {0x38, 0x40, 0x40, 0x78, 0x44, 0x44, 0x44, 0x38}, /* 6 */
    {0x7C, 0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x10}, /* 7 */
    {0x38, 0x44, 0x44, 0x38, 0x44, 0x44, 0x44, 0x38}, /* 8 */
    {0x38, 0x44, 0x44, 0x44, 0x3C, 0x04, 0x04, 0x38}, /* 9 */

    /* Symbols. */
    {0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x00}, /* : */
    {0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x10}, /* ; */
    {0x00, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04}, /* < */
    {0x00, 0x00, 0x00, 0x7C, 0x00, 0x7C, 0x00, 0x00}, /* = */
    {0x00, 0x40, 0x20, 0x10, 0x08, 0x10, 0x20, 0x40}, /* > */
    {0x38, 0x44, 0x04, 0x08, 0x10, 0x10, 0x00, 0x10}, /* ? */ 
    {0x38, 0x44, 0x4c, 0x54, 0x54, 0x4c, 0x40, 0x38}, /* @ */
    
    /* Uppercase letters. */
    {0x10, 0x28, 0x28, 0x44, 0x7c, 0x44, 0x44, 0x44}, /* A */ 
    {0x78, 0x44, 0x44, 0x78, 0x44, 0x44, 0x44, 0x78}, /* B */ 
    {0x38, 0x44, 0x40, 0x40, 0x40, 0x40, 0x44, 0x38}, /* C */
    {0x70, 0x48, 0x44, 0x44, 0x44, 0x44, 0x48, 0x70}, /* D */ 
    {0x7C, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x7C}, /* E */ 
    {0x7C, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x40}, /* F */
    {0x38, 0x44, 0x40, 0x40, 0x40, 0x4C, 0x44, 0x38}, /* G */ 
    {0x44, 0x44, 0x44, 0x7C, 0x44, 0x44, 0x44, 0x44}, /* H */ 
    {0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38}, /* I */
    {0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x90, 0x60}, /* J */ 
    {0x48, 0x48, 0x50, 0x60, 0x50, 0x50, 0x48, 0x48}, /* K */ 
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7C}, /* L */
    {0x82, 0x82, 0xC6, 0xC6, 0xAA, 0xAA, 0x92, 0x92}, /* M */ 
    {0x44, 0x64, 0x64, 0x54, 0x54, 0x4C, 0x4C, 0x44}, /* N */ 
    {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C}, /* O */
    {0x38, 0x24, 0x24, 0x24, 0x38, 0x20, 0x20, 0x20}, /* P */ 
    {0x3C, 0x42, 0x42, 0x42, 0x42, 0x46, 0x42, 0x3D}, /* Q */ 
    {0x38, 0x24, 0x24, 0x24, 0x38, 0x24, 0x24, 0x24}, /* R */
    {0x38, 0x44, 0x40, 0x38, 0x04, 0x04, 0x44, 0x38}, /* S */ 
    {0x7C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10}, /* T */ 
    {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C}, /* U */
    {0x44, 0x44, 0x44, 0x28, 0x28, 0x28, 0x10, 0x10}, /* V */ 
    {0x92, 0x92, 0x92, 0x92, 0x54, 0x54, 0x28, 0x28}, /* W */ 
    {0x44, 0x44, 0x28, 0x10, 0x28, 0x28, 0x44, 0x44}, /* X */
    {0x44, 0x44, 0x28, 0x28, 0x10, 0x10, 0x10, 0x10}, /* Y */ 
    {0x7C, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x7C}, /* Z */
    
    /* Symbols. */
    {0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38}, /* [ */ 
    {0x40, 0x20, 0x20, 0x10, 0x10, 0x08, 0x08, 0x04}, /* \ */ 
    {0x38, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x38}, /* ] */
    {0x10, 0x28, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ^ */ 
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C}, /* _ */ 
    {0x20, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ` */
    
    /* Lowercase letters. */
    {0x00, 0x00, 0x38, 0x04, 0x3C, 0x44, 0x44, 0x38}, /* a */ 
    {0x20, 0x20, 0x20, 0x38, 0x24, 0x24, 0x24, 0x38}, /* b */ 
    {0x00, 0x00, 0x38, 0x44, 0x40, 0x40, 0x44, 0x38}, /* c */
    {0x04, 0x04, 0x04, 0x1C, 0x24, 0x24, 0x24, 0x1C}, /* d */ 
    {0x00, 0x00, 0x38, 0x44, 0x44, 0x78, 0x40, 0x38}, /* e */ 
    {0x18, 0x20, 0x78, 0x20, 0x20, 0x20, 0x20, 0x20}, /* f */
    {0x0C, 0x38, 0x44, 0x38, 0x40, 0x38, 0x44, 0x38}, /* g */ 
    {0x40, 0x40, 0x40, 0x78, 0x44, 0x44, 0x44, 0x44}, /* h */ 
    {0x00, 0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x38}, /* i */
    {0x00, 0x08, 0x00, 0x18, 0x08, 0x08, 0x48, 0x30}, /* j */ 
    {0x40, 0x40, 0x48, 0x48, 0x50, 0x60, 0x50, 0x48}, /* k */ 
    {0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38}, /* l */
    {0x00, 0x00, 0xEC, 0x92, 0x92, 0x92, 0x92, 0x92}, /* m */ 
    {0x00, 0x00, 0x38, 0x24, 0x24, 0x24, 0x24, 0x24}, /* n */ 
    {0x00, 0x00, 0x38, 0x44, 0x44, 0x44, 0x44, 0x38}, /* o */
    {0x00, 0x00, 0x5C, 0x22, 0x22, 0x3C, 0x20, 0x20}, /* p */ 
    {0x00, 0x00, 0x3A, 0x44, 0x44, 0x3C, 0x04, 0x04}, /* q */ 
    {0x00, 0x00, 0x38, 0x44, 0x40, 0x40, 0x40, 0x40}, /* r */
    {0x00, 0x00, 0x38, 0x40, 0x38, 0x04, 0x04, 0x38}, /* s */ 
    {0x20, 0x20, 0x78, 0x20, 0x20, 0x20, 0x20, 0x18}, /* t */ 
    {0x00, 0x00, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38}, /* u */
    {0x00, 0x00, 0x44, 0x44, 0x28, 0x28, 0x10, 0x10}, /* v */ 
    {0x00, 0x00, 0x92, 0x92, 0x92, 0x54, 0x28, 0x28}, /* w */ 
    {0x00, 0x00, 0x44, 0x28, 0x10, 0x10, 0x28, 0x44}, /* x */
    {0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x10, 0x20}, /* y */ 
    {0x00, 0x00, 0x7C, 0x08, 0x10, 0x10, 0x20, 0x7C}, /* z */
    
    /* Symbols. */
    {0x18, 0x20, 0x20, 0x40, 0x40, 0x20, 0x20, 0x18}, /* { */ 
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10}, /* | */ 
    {0x30, 0x08, 0x08, 0x04, 0x04, 0x08, 0x08, 0x30}, /* } */ 
    {0x00, 0x00, 0x00, 0x60, 0x92, 0x0C, 0x00, 0x00}, /* ~ */ 

    /* Invalid characters. */
    UNKNOWNMARK
};

void TextIoClear()
{
    for (ULONG_PTR y = gMinY; y < gMaxY; y++)
    {
        for (ULONG_PTR x = gMinX; x < gMaxX; x++)
        {
            gFramebuffer[x + y * gPixelsPerScanline] = gBackdrop;
        }
    }
}

void TextIoSetBoundingBox(UINT32 *boundingBox)
{
    gMinX = boundingBox[0];
    gMaxX = boundingBox[1];
    gMinY = boundingBox[2];
    gMaxY = boundingBox[3];
}

void TextIoGetBoundingBox(UINT32 *boundingBox)
{
    boundingBox[0] = gMinX;
    boundingBox[1] = gMaxX;
    boundingBox[2] = gMinY;
    boundingBox[3] = gMaxY;
}

void TextIoSetCursorPosition(UINT32 posX, UINT32 posY)
{
    gCursorX = posX;
    gCursorY = posY;
}

void TextIoGetCursorPosition(UINT32* posX, UINT32* posY)
{
    *posX = gCursorX;
    *posY = gCursorY;
}

void TextIoSetColorInformation(UINT32 color, UINT32 background, UINT8 renderBack)
{
    gColor = color;
    gBackdrop = background;
    gRenderBackdrop = renderBack;
}

void TextIoGetColorInformation(UINT32 *color, UINT32* background, UINT8 *renderBack)
{
    *color = gColor;
    *background = gBackdrop;
    *renderBack = gRenderBackdrop;
}
void TextIoSetAlignment(UINT8 alignment)
{
    align = alignment;
}

void TextIoInitialize(
    volatile UINT32* framebufferIn,
    volatile UINT32* framebufferEndIn, UINT32 w, UINT32 h, UINT32 p)
{
    if (initialized && w == 0)
        w = gWidth;

    if (initialized && h == 0)
        h = gHeight;

    if (initialized && p == 0)
        p = gPixelsPerScanline;

    if (!initialized)
    {
        UINT32 defBoundingBox[] = { 0, gWidth, 0, gHeight };
        TextIoSetBoundingBox(defBoundingBox);
    }

    gFramebuffer = framebufferIn;
    gFramebufferEnd = framebufferEndIn;

    gWidth = w;
    gHeight = h;
    gPixelsPerScanline = p;
    initialized = 0xFF;

    TextIoSetAlignment(0);
    TextIoSetCursorPosition(1, 1);
    TextIoSetColorInformation(0xFFFFFFFF, 0, 1);
}

UINT8 TextIoGetAlignment()
{
    return align;
}

UINT8 TextIoIsInitialized()
{
    return initialized == 0xFF;
}

void TextIoOutputCharacterWithinBox(
    UINT8 characterID, UINT32 posX, UINT32 posY, UINT32 color, UINT32 backdrop, UINT8 renderBackdrop,
    UINT32 minX, UINT32 maxX, UINT32 minY, UINT32 maxY)
{
    if (!TextIoIsInitialized())
        return;

    if (characterID >= sizeof(StaticFont8x8) / sizeof(*StaticFont8x8))
    {
        characterID = (sizeof(StaticFont8x8) / sizeof(*StaticFont8x8)) - 1;
    }
    UINT8 *StaticFontEntry = StaticFont8x8[characterID];

    for (int y = 0; y < 8; y++)
    {

        UINT8 rowData = StaticFontEntry[y];

        for (int x = 0; x < 8; x++)
        {
            if ((7 - x) + posX <= maxX && (7 - x) + posX >= minX && y + posY <= maxY && y + posY >= minY)
            {
                if (GetBit(rowData, x))
                {
                    gFramebuffer[(7 - x) + posX + (y + posY) * gPixelsPerScanline] = color;
                }
                else if (renderBackdrop)
                {
                    gFramebuffer[(7 - x) + posX + (y + posY) * gPixelsPerScanline] = backdrop;
                }
            }
        }
    }
}

void TextIoMoveUpARow()
{

    for (UINT64 y = gMinY; y < gMaxY - 9; y++)
    {
        for (UINT64 x = gMinX; x < gMaxX; x++)
        {
            gFramebuffer[gPixelsPerScanline*y + x] = gFramebuffer[gPixelsPerScanline*(y + 9) + x];
            gFramebuffer[gPixelsPerScanline*(y + 9) + x] = gBackdrop;
        }
    }
    gCursorY -= 9;
}

void TextIoOutputCharacter(UINT8 characterID, UINT32 posX, UINT32 posY, UINT32 color, UINT32 backdrop, UINT8 renderBackdrop)
{
    if (!TextIoIsInitialized())
        return;
    TextIoOutputCharacterWithinBox(characterID, posX, posY, color, backdrop, renderBackdrop, gMinX, gMaxX, gMinY, gMaxY);
}

void TextIoOutputString(
    const char* input, UINT32 posX, UINT32 posY, UINT32 color, UINT32 backdrop,
    UINT8 renderBackdrop, UINT32 minX, UINT32 maxX, UINT32 minY, UINT32 maxY)
{
    if (!TextIoIsInitialized())
        return;

    UINT32 initialPX = posX, initialPY = posY;
    int stringIndex = 0;

    TextIoDeltaX = 0;
    TextIoDeltaY = 0;

    while (input[stringIndex])
    {
        if (posX + 8 > maxX || input[stringIndex] == '\n')
        {
            posX = minX;
            posY += 9;
        }
        if (posY + 8 > maxY)
        {
            TextIoDeltaX = posX - initialPX;
            TextIoDeltaY = posY - initialPY;
            if (IsCurrentOperationGlobal)
            {
                IsCurrentOperationGlobal = FALSE;
                gCursorX = posX;
                gCursorY = posY;
            }
            return;
        }

        if (input[stringIndex] == '\n')
        {
            TextIoDeltaX = posX - initialPX;
            TextIoDeltaY = posY - initialPY;
            if (IsCurrentOperationGlobal)
            {
                IsCurrentOperationGlobal = FALSE;
                gCursorX = posX;
                gCursorY = posY;
            }
            return;
        }

        if (align)
        {
            posX += (align - 1);
            posX /= align;
            posX *= align;
        }

        TextIoOutputCharacterWithinBox(input[stringIndex], posX, posY, color, backdrop, renderBackdrop, minX, maxX, minY, maxY);
        if (align)
            posX += align;
        else
            posX += 8;
        stringIndex++;
    }

    TextIoDeltaX = posX - initialPX;
    TextIoDeltaY = posY - initialPY;
    if (IsCurrentOperationGlobal)
    {
        IsCurrentOperationGlobal = FALSE;
        gCursorX = posX;
        gCursorY = posY;
    }
}

void TextIoOutputStringGlobal(const char* input)
{
    IsCurrentOperationGlobal = TRUE;
    TextIoOutputString(input, gCursorX, gCursorY, gColor, gBackdrop, gRenderBackdrop, gMinX, gMaxX, gMinY, gMaxY);
}

void TextIoOutputFormatedString(const char* input, SIZE_T size, va_list args2)
{

    void* args = args2;

    for (UINT32 i = 0; i < size; i++)
    {
        if (gCursorX + 8 > gMaxX || input[i] == '\n')
        {
            gCursorX = gMinX;
            gCursorY += 9;
        }
        if (gCursorY + 8 > gMaxY)
        {
            TextIoMoveUpARow();
        }

        if (align)
        {
            gCursorX += (align - 1);
            gCursorX /= align;
            gCursorX *= align;
        }

        switch (input[i])
        {
            case '\n':
                break;
            case '%':
                i++;

                switch (input[i])
                {
                    case '%':
                        goto display;
                    case 's':;
                        char* toDisplay = (char*)*((UINT64*) args);
                        args = ((UINT64*) args) + 1;
                        TextIoOutputStringGlobal(toDisplay);
                        break;
                    case 'S':
                    {
                        char* toDisplay = (char*)*((UINT64*) args);
                        args = ((UINT64*) args) + 1;
                        UINT64 lenght = *((UINT64*) args);
                        args = ((UINT64*) args) + 1;
                        for (UINT64 a = 0; a < lenght; a++)
                        {
                            char string[2] = { 0,0 };
                            string[0] = toDisplay[a];
                            TextIoOutputStringGlobal(string);
                        }
                        break;
                    }
                    case 'U':
                    {
                        PUNICODE_STRING pStr = (PUNICODE_STRING)*((UINT64*)args);
                        args = ((UINT64*)args) + 1;
                        
                        for (int i = 0; i < pStr->Length / 2; i++)
                        {
                            char buffer[2] = { 0,0 };
                            buffer[0] = (char)pStr->Buffer[i];
                            TextIoOutputStringGlobal(buffer);
                        }
                        break;
                    }
                    case 'c':;
                        char string[2] = { 0,0 };
                        string[0] = (char)*((UINT64*) args);
                        if (string[0] == 0)
                            string[0] = 0xff;
                        ((UINT64*) args) += 1;
                        TextIoOutputStringGlobal(string);
                        break;
                    case 'd':
                    case 'i':
                    {
                        UINT64 c = *((UINT64*) args);
                        args = ((UINT64*) args) + 1;
                        char str[32] = { 0 };
                        IntegerToAscii(c, 10, str);
                        TextIoOutputStringGlobal(str);
                        break;
                    }
                    case 'b':
                    case 'B':
                    {
                        UINT64 c = *((UINT64*) args);
                        args = ((UINT64*) args) + 1;
                        char str[65] = { 0 };
                        IntegerToAscii(c, 2, str);
                        TextIoOutputStringGlobal(str);
                        break;
                    }
                    case 'H':
                    {
                        UINT64 c = *((UINT64*) args);
                        UINT64 length;
                        char str[17] = { 0 };
                        char str2[] = "0000000000000000";
                        args = ((UINT64*) args) + 1;
                        IntegerToAsciiCapital(c, 16, str);
                        length = FindCharacterFirst(str, -1, 0);
                        RtlCopyMemory(str2 + 16 - length, str, length);
                        TextIoOutputStringGlobal(str2);
                        break;
                    }
                    case 'X':
                    {
                        UINT64 c = *((UINT64*) args);
                        args = ((UINT64*) args) + 1;
                        char str[32] = { 0 };
                        IntegerToAsciiCapital(c, 16, str);
                        TextIoOutputStringGlobal(str);
                        break;
                    }
                    case 'x':
                    {
                        UINT64 c = *((UINT64*) args);
                        args = ((UINT64*) args) + 1;
                        char str[32] = { 0 };
                        IntegerToAscii(c, 16, str);
                        TextIoOutputStringGlobal(str);
                        break;
                    }
                    default:
                        TextIoOutputStringGlobal("?");
                        break;
                }
                break;
            default:
            display:
                TextIoOutputCharacterWithinBox(input[i], gCursorX, gCursorY, gColor, gBackdrop, gRenderBackdrop, gMinX, gMaxX, gMinY, gMaxY);
                gCursorX += 8;
                break;
        }

    }
}

static KSPIN_LOCK PrintLock;

void PrintTA(const char* input, ...)
{
    va_list        args;
    va_start(args, input);

    KIRQL oldIrql = KeGetCurrentIrql();

    if (oldIrql < DISPATCH_LEVEL)
        KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    
    KiAcquireSpinLock((volatile ULONG_PTR*)&PrintLock);
    TextIoOutputFormatedString(input, FindCharacterFirst(input, -1, 0), args);
    KiReleaseSpinLock((volatile ULONG_PTR*)&PrintLock);

    if (oldIrql < DISPATCH_LEVEL)
        KeLowerIrql(oldIrql);
        
    va_end(args);
}

void TextIoTest(UINT64 mode)
{
    if (!TextIoIsInitialized())
        return;

    if (mode == 0)
    {
        TextIoOutputCharacter('!', 20, 20, 0xFF000000, 0, 0);
        TextIoOutputCharacter('~', 28, 20, 0xFFFFFFFF, 0, 1);
        TextIoOutputCharacter('}', 36, 20, 0xFF000000, 0, 0);

        TextIoOutputString("test of the new ............................................................................................................................................\nhhh", 500, 20, 0xFFBFBFBF, 0, 1, 0, 550, 0, gHeight);

        TextIoOutputString("Alignment test\n", 20, 100, 0xFFbFbfbF, 0, 1, 0, gWidth, 0, gHeight);
        PrintT("test %i 0x%X %x %x %c '%s' %i '%s'\n", 185, 0x666666, 0x12345678, 0xabcdef00, '*', "g", 0x69LL, "mn");

    }
}
