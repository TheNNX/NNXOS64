#ifndef NNX_STIO_HEADER
#define NNX_STIO_HEADER
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
	void TextIOInitialize(int* framebufferIn, int* framebufferEndIn, UINT32 w, UINT32 h, UINT32 p);
	void TextIOOutputCharacter(UINT8 characterID, UINT32 posX, UINT32 posY, UINT32 color, UINT32 backdrop, UINT8 renderBackdrop);
	void TextIOOutputFormatedString(char* input, UINT32 size, va_list args2);
	void TextIOTest(UINT64 mode);
	UINT8 TextIOGetAlignment();
	UINT8 TextIOIsInitialized();
	void TextIOOutputCharacterWithinBox(UINT8 characterID, UINT32 posX, UINT32 posY, UINT32 color, UINT32 backdrop, UINT8 renderBackdrop, UINT32 minX, UINT32 maxX, UINT32 minY, UINT32 maxY);
	void PrintTA(const char* input, ...);
	void TextIOOutputStringGlobal(const char* input);
	void TextIOOutputString(const char* input, UINT32 posX, UINT32 posY, UINT32 color, UINT32 backdrop, UINT8 renderBackdrop, UINT32 minX, UINT32 maxX, UINT32 minY, UINT32 maxY);
#ifdef VERBOSE
#define PrintT PrintTA("<%s %i>: ",__FILE__, __LINE__);PrintTA
#else
#define PrintT PrintTA
#endif

#ifdef PRINT_IN_DEBUG_ONLY
#ifndef DEBUG
	inline void null(const char* input, ...) {
		return;
	}
#define PrintT null
#endif
#endif

	
	
	void TextIOClear();
	UINT64 FrameBufferSize();

	extern UINT32* framebuffer;
	extern UINT32* framebufferEnd;
	extern UINT32 TextIODeltaX;
	extern UINT32 TextIODeltaY;

#define FRAMEBUFFER_DESIRED_LOCATION 0x80000000

#ifdef __cplusplus
	}
#endif
#endif
