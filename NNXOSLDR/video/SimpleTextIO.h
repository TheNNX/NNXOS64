#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "nnxint.h"
#include "nnxarg.h"

	void TextIOSetBoundingBox(UINT32 *boundingBox);
	void TextIOGetBoundingBox(UINT32 *boundingBox);
	void TextIOSetCursorPosition(UINT32 posX, UINT32 posY);
	void TextIOGetCursorPosition(UINT32* posX, UINT32* posY);
	void TextIOSetColorInformation(UINT32 color, UINT32 background, UINT8 renderBack);
	void TextIOGetColorInformation(UINT32 *color, UINT32* background, UINT8 *renderBack);
	void TextIOSetAlignment(UINT8 alignment);
	void TextIOInitialize(int* framebufferIn, int* framebufferEndIn, UINT32 w, UINT32 h);
	void TextIOOutputCharacter(UINT8 characterID, UINT32 posX, UINT32 posY, UINT32 color, UINT32 backdrop, UINT8 renderBackdrop);
	void TextIOOutputFormatedString(const char* input, UINT32 size, va_list args);
	void TextIOTest(UINT64 mode);
	UINT8 TextIOGetAlignment();
	UINT8 TextIOIsInitialized();
	void TextIOOutputCharacterWithinBox(UINT8 characterID, UINT32 posX, UINT32 posY, UINT32 color, UINT32 backdrop, UINT8 renderBackdrop, UINT32 minX, UINT32 maxX, UINT32 minY, UINT32 maxY);
	void PrintT(const char* input, ...);
	void TextIOClear();
	UINT64 FrameBufferSize();

	extern UINT32* framebuffer;
	extern UINT32* framebufferEnd;

#define FRAMEBUFFER_DESIRED_LOCATION 0x80000000

#ifdef __cplusplus
	}
#endif 