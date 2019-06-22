
#include  "SimpleTextIO.h"
#include "memory/physical_allocator.h"

int* framebuffer;
int* framebufferEnd;

UINT32 width;
UINT32 height;

UINT8 align = 0;

UINT32 gCursorX = 0, gCursorY = 0, gColor = 0, gBackdrop = 0;
UINT8 gRenderBackdrop = 0;

UINT32 gMinX = 0, gMaxX = 0;
UINT32 gMinY = 0, gMaxY = 0;

UINT8 initialized = 0;

UINT8 StaticFont8x8[][8] = { {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}, //unprintables

//symbols and util
{0,0,0,0,0,0,0,0} /*space*/, {0x10,0x10,0x10,0x10,0x10,0x10,0x0,0x10}/*!*/, {0x28,0x28,0x28,0,0,0,0,0}/*"*/, {0x28,0x28,0x7C,0x28,0x28,0x7C,0x28,0x28}/*#*/,  
{0x38,0x54,0x50,0x38,0x14,0x14,0x54,0x38}/*$*/, {0x44,0x8,0x8,0x10,0x20,0x20,0x44} /*%*/, {0x38, 0x44, 0x44, 0x38, 0x44, 0x46, 0x44, 0x3A}/*&*/, {0x10,0x10,0x10,0,0,0,0,0}/*'*/, 
{0x8, 0x10, 0x20, 0x20, 0x20, 0x20, 0x10, 0x8}/*(*/,{0x20,0x10,0x8,0x8,0x08,0x8,0x10,0x20}/*)*/, {0x10, 0x7C, 0x38, 0x6c, 0, 0, 0, 0}/* * */, {0,0, 0x10, 0x10, 0x7C, 0x10, 0x10, 0}/*+*/, 
{0,0,0,0,0,0,0x10,0x10}/*,*/, {0,0,0,0,0x7c,0,0,0}/*-*/, {0,0,0,0,0,0,0,0x10}/*.*/,  {0x4,0x8,0x8,0x10,0x10,0x20,0x20,0x40}/*/*/, 
//numbers
/*0*/ {0x38, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38},
/*1*/ {0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38},
/*2*/ {0x38, 0x44, 0x04, 0x04, 0x08, 0x10, 0x20, 0x7C},
/*3*/ {0x38, 0x04, 0x04, 0x38, 0x04, 0x04, 0x04, 0x38},
/*4*/ {0x10, 0x30, 0x50, 0x90, 0xF8, 0x10, 0x10, 0x10},
/*5*/ {0x7C, 0x40, 0x40, 0x78, 0x04, 0x04, 0x04, 0x78},
/*6*/ {0x38, 0x40, 0x40, 0x78, 0x44, 0x44, 0x44, 0x38},
/*7*/ {0x7C, 0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x10},
/*8*/ {0x38, 0x44, 0x44, 0x38, 0x44, 0x44, 0x44, 0x38},
/*9*/ {0x38, 0x44, 0x44, 0x44, 0x3C, 0x04, 0x04, 0x38},
//symbols and utils 2
{0,0,0,0x10,0,0,0x10,0}/*:*/,{0,0,0,0x10,0,0,0x10,0x10}/*;*/,{0, 0x04, 0x08, 0x10, 0x20, 0x010, 0x08, 0x04}/*<*/, {0,0,0,0x7C,0,0x7C,0,0}/*=*/, {0, 0x40, 0x20, 0x10, 0x08, 0x10,0x20, 0x40}/*>*/,
{0x38, 0x44, 0x04, 0x08, 0x10, 0x10, 0, 0x10}/*?*/, {0x38, 0x44, 0x4c, 0x54, 0x54, 0x4c, 0x40, 0x38}/*@*/,
//uppercase letters
{0x10, 0x28, 0x28, 0x44, 0x7c, 0x44, 0x44, 0x44}/*A*/, {0x78, 0x44, 0x44, 0x78, 0x44, 0x44, 0x44, 0x78}/*B*/, {0x38, 0x44, 0x40, 0x40, 0x40, 0x40, 0x44, 0x38}/*C*/,
{0x70, 0x48, 0x44, 0x44, 0x44, 0x44, 0x48, 0x70}/*D*/, {0x7C, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x7C}/*E*/, {0x7C, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x40}/*F*/,
{0x38, 0x44, 0x40, 0x40, 0x40, 0x4C, 0x44, 0x38}/*G*/, {0x44, 0x44, 0x44, 0x7C, 0x44, 0x44, 0x44, 0x44}/*H*/, {0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38}/*I*/,
{0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x90, 0x60}/*J*/, {0x48, 0x48, 0x50, 0x60, 0x50, 0x50, 0x48, 0x48}/*K*/, {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7C}/*L*/,
{0x82, 0x82, 0xC6, 0xC6, 0xAA, 0xAA, 0x92, 0x92}/*M*/, {0x44, 0x64, 0x64, 0x54, 0x54, 0x4C, 0x4C, 0x44}/*N*/, {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C}/*O*/,
{0x38, 0x24, 0x24, 0x24, 0x38, 0x20, 0x20, 0x20}/*P*/, {0x3C, 0x42, 0x42, 0x42, 0x42, 0x46, 0x42, 0x3D}/*Q*/, {0x38, 0x24, 0x24, 0x24, 0x38, 0x24, 0x24, 0x24}/*R*/,
{0x38, 0x44, 0x40, 0x38, 0x04, 0x04, 0x44, 0x38}/*S*/, {0x7C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10}/*T*/, {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C}/*U*/,
{0x44, 0x44, 0x44, 0x28, 0x28, 0x28, 0x10, 0x10}/*V*/, {0x92, 0x92, 0x92, 0x92, 0x54, 0x54, 0x28, 0x28}/*W*/, {0x44, 0x44, 0x28, 0x10, 0x28, 0x28, 0x44, 0x44}/*X*/,
{0x44, 0x44, 0x28, 0x28, 0x10, 0x10, 0x10, 0x10}/*Y*/, {0x7C, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x7C}/*Z*/,
//symbols and utils 3
{0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38}/*[*/, {0x40, 0x20, 0x20, 0x10, 0x10, 0x08, 0x08, 0x40}/*\*/, {0x38, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x38}/*]*/,
{0x10, 0x28, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00}/*^*/, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C}/*_*/, {0x20, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00}/*`*/,
//lowercase letters
{0x00, 0x00, 0x38, 0x04, 0x3C, 0x44, 0x44, 0x38}/*a*/, {0x20, 0x20, 0x20, 0x38, 0x24, 0x24, 0x24, 0x38}/*b*/, {0x00, 0x00, 0x38, 0x44, 0x40, 0x40, 0x44, 0x38}/*c*/,
{0x04, 0x04, 0x04, 0x1C, 0x24, 0x24, 0x24, 0x1C}/*d*/, {0x00, 0x00, 0x38, 0x44, 0x44, 0x78, 0x40, 0x38}/*e*/, {0x18, 0x20, 0x78, 0x20, 0x20, 0x20, 0x20, 0x20}/*f*/,
{0x0C, 0x38, 0x44, 0x38, 0x40, 0x38, 0x44, 0x38}/*g*/, {0x40, 0x40, 0x40, 0x78, 0x44, 0x44, 0x44, 0x44}/*h*/, {0x00, 0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x38}/*i*/,
{0x00, 0x10, 0x00, 0x30, 0x10, 0x10, 0x50, 0x20}/*j*/, {0x40, 0x40, 0x48, 0x48, 0x50, 0x60, 0x50, 0x48}/*k*/, {0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38}/*l*/,
{0x00, 0x00, 0xEC, 0x92, 0x92, 0x92, 0x92, 0x92}/*m*/, {0x00, 0x00, 0x38, 0x24, 0x24, 0x24, 0x24, 0x24}/*n*/, {0x00, 0x00, 0x38, 0x44, 0x44, 0x44, 0x44, 0x38}/*o*/,
{0x00, 0x00, 0x1C, 0x22, 0x22, 0x3C, 0x20, 0x20}/*p*/, {0x00, 0x00, 0x38, 0x44, 0x44, 0x3C, 0x04, 0x04}/*q*/, {0x00, 0x00, 0x38, 0x44, 0x40, 0x40, 0x40, 0x40}/*r*/,
{0x00, 0x00, 0x38, 0x40, 0x38, 0x04, 0x04, 0x38}/*s*/, {0x20, 0x20, 0x78, 0x20, 0x20, 0x20, 0x20, 0x18}/*t*/, {0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C}/*u*/,
{0x00, 0x00, 0x44, 0x44, 0x28, 0x28, 0x10, 0x10}/*v*/, {0x00, 0x00, 0x92, 0x92, 0x92, 0x54, 0x28, 0x28}/*w*/, {0x00, 0x00, 0x44, 0x28, 0x10, 0x10, 0x28, 0x44}/*x*/,
{0x00, 0x00, 0x44, 0x44, 0x28, 0x10, 0x20, 0x40}/*y*/, {0x00, 0x00, 0x7C, 0x08, 0x10, 0x10, 0x20, 0x7C}/*z*/,
//symbols and utils 4
{0x18, 0x20, 0x20, 0x40, 0x40, 0x20, 0x20, 0x18}/*{*/, {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10}/*|*/, {0x30,0x08,0x8,0x04,0x04,0x8,0x08,0x30}/*}*/, {0,0,0,0x60,0x92,0xC,0,0}/*~*/, {0,0,0,0,0,0,0,0}/*DEL*/
};

char* itoa(UINT64 i, UINT8 base, char b[]) {
	char const digit[] = "0123456789abcdef";
	char* p = b;

	UINT64 shifter = i;
	do {
		++p;
		shifter = shifter / base;
	} while (shifter);

	*p = '\0';

	do {
		*--p = digit[i%base];
		i = i / base;
	} while (i);
	return b;
}

char* itoaC(UINT64 i, UINT8 base, char b[]) {
	char const digit[] = "0123456789ABCDEF";
	char* p = b;

	UINT64 shifter = i;
	do {
		++p;
		shifter = shifter / base;
	} while (shifter);
	*p = '\0';
	do {
		*--p = digit[i%base];
		i = i / base;
	} while (i);
	return b;
}

void TextIOClear() {
	for (int y = gMinY; y < gMaxY; y++) {
		for (int x = gMinX; x < gMaxX; x++) {
			framebuffer[x + y * width] = 0;
		}
	}
}

void TextIOSetBoundingBox(UINT32 *boundingBox) {
	gMinX = boundingBox[0];
	gMaxX = boundingBox[1];
	gMinY = boundingBox[2];
	gMaxY = boundingBox[3];
}

void TextIOGetBoundingBox(UINT32 *boundingBox) {
	boundingBox[0] = gMinX;
	boundingBox[1] = gMaxX;
	boundingBox[2] = gMinY;
	boundingBox[3] = gMaxY;
}

void TextIOSetCursorPosition(UINT32 posX, UINT32 posY) {
	gCursorX = posX;
	gCursorY = posY;
}

void TextIOGetCursorPosition(UINT32* posX, UINT32* posY) {
	*posX = gCursorX;
	*posY = gCursorY;
}

void TextIOSetColorInformation(UINT32 color, UINT32 background, UINT8 renderBack) {
	gColor = color;
	gBackdrop = background;
	gRenderBackdrop = renderBack;
}

void TextIOGetColorInformation(UINT32 *color, UINT32* background, UINT8 *renderBack) {
	*color = gColor;
	*background = gBackdrop;
	*renderBack = gRenderBackdrop;
}
void TextIOSetAlignment(UINT8 alignment) {
	align = alignment;
}

void TextIOInitialize(int* framebufferIn, int* framebufferEndIn, UINT32 w, UINT32 h) {
	framebuffer = framebufferIn;
	framebufferEnd = framebufferEndIn;
	width = w;
	height = h;
	initialized = 0xFF;
	
	TextIOSetAlignment(0);
	TextIOSetCursorPosition(0,0);
	TextIOSetColorInformation(0xFFFFFFFF, 0, 1);
	UINT32 defBoundingBox[4] = { 0 };
	defBoundingBox[1] = width;
	defBoundingBox[3] = height;
	TextIOSetBoundingBox(defBoundingBox);
}


UINT8 TextIOGetAlignment() {
	return align;
}

UINT8 TextIOIsInitialized() {
	return initialized == 0xFF;
}

void TextIOOutputCharacterWithinBox(UINT8 characterID, UINT32 posX, UINT32 posY, UINT32 color, UINT32 backdrop, UINT8 renderBackdrop, UINT32 minX, UINT32 maxX, UINT32 minY, UINT32 maxY) {
	if (!TextIOIsInitialized())
		return;
	UINT8 *StaticFontEntry = StaticFont8x8[characterID];

	for (int y = 0; y < 8; y++) {

		UINT8 rowData = StaticFontEntry[y];

		for (int x = 0; x < 8; x++) {
			if ((7 - x) + posX <= maxX && (7 - x) + posX >= minX && y + posY <= maxY && y + posY >= minY) {
				if (GetBit(rowData, x)) {
					framebuffer[(7 - x) + posX + (y + posY) * width] = color;
				}
				else if (renderBackdrop) {
					framebuffer[(7 - x) + posX + (y + posY) * width] = backdrop;
				}
			}

		}
	}
}

void TextIOOutputCharacter(UINT8 characterID, UINT32 posX, UINT32 posY, UINT32 color, UINT32 backdrop, UINT8 renderBackdrop) {
	if (!TextIOIsInitialized())
		return;
	TextIOOutputCharacterWithinBox(characterID, posX, posY, color, backdrop, renderBackdrop, 0, width, 0, height);
}

void TextIOOutputStringGlobal(const char* input) {
	if (!TextIOIsInitialized())
		return;

	int stringIndex = 0;

	while (input[stringIndex])
	{
		if (gCursorX + 8 > gMaxX || input[stringIndex] == '\n') {
			gCursorX = gMinX;
			gCursorY += 9;
		}
		if (gCursorY + 8 > gMaxY) {
			return;
		}

		if (align)
		{
			gCursorX+=(align-1);
			gCursorX /= align;
			gCursorX *= align;
		}

		TextIOOutputCharacterWithinBox(input[stringIndex], gCursorX, gCursorY, gColor, gBackdrop, gRenderBackdrop, gMinX, gMaxX, gMinY, gMaxY);
		if (align)
			gCursorX += align;
		else
			gCursorX += 8;
		stringIndex++;
	}
}

void TextIOOutputString(const char* input, UINT32 posX, UINT32 posY, UINT32 color, UINT32 backdrop, UINT8 renderBackdrop, UINT32 minX, UINT32 maxX, UINT32 minY, UINT32 maxY) {
	if (!TextIOIsInitialized())
		return;

	UINT32 cursorX = posX;
	UINT32 cursorY = posY;

	int stringIndex = 0;

	while (input[stringIndex])
	{
		if (cursorX + 8 > maxX || input[stringIndex] == '\n') {
			cursorX = minX;
			cursorY += 9;
		}
		if (cursorY + 8 > maxY) {
			return;
		}

		if (align)
		{
			cursorX += (align - 1);
			cursorX /= align;
			cursorX *= align;
		}

		TextIOOutputCharacterWithinBox(input[stringIndex], cursorX, cursorY, color, backdrop, renderBackdrop, minX, maxX, minY, maxY);
		if (align)
			cursorX += align;
		else
			cursorX += 8;
		stringIndex++;
	}
}

unsigned int __strlen(const char* input) {
	unsigned int result = 0;
	while (input[result]) 
		result++;
	return result;
}

void TextIOOutputFormatedString(char input[], UINT32 size, va_list args2) {
	
	void* args = args2;
	
	for (UINT32 i = 0; i < size; i++) {
		

		if (gCursorX + 8 > gMaxX || input[i] == '\n') {
			gCursorX = gMinX;
			gCursorY += 9;
		}
		if (gCursorY + 8 > gMaxY) {
			return;
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
						char* toDisplay = *((UINT64*)args);
						((UINT64*)args) += 1;
						TextIOOutputStringGlobal(toDisplay);
						break;
					case 'c':;
						char string[2] = {0,0};
						string[0] = *((UINT64*)args);
						((UINT64*)args) += 1;
						TextIOOutputStringGlobal(string);
						break;
					case 'd':
					case 'i': {
						UINT64 c = *((UINT64*)args);
						((UINT64*)args) += 1;
						char str[32] = { 0 };
						itoa(c, 10, str);
						TextIOOutputStringGlobal(str);
						break;
					}
					case 'B': {
						UINT64 c = *((UINT64*)args);
						((UINT64*)args) += 1;
						char str[32] = { 0 };
						itoa(c, 2, str);
						TextIOOutputStringGlobal(str);
						break;
					}
					case 'X': {
						UINT64 c = *((UINT64*)args);
						((UINT64*)args) += 1;
						char str[32] = { 0 };
						itoaC(c, 16, str);
						TextIOOutputStringGlobal(str);
						break;
					}
					case 'x': {
						UINT64 c = *((UINT64*)args);
						((UINT64*)args) += 1;
						char str[32] = { 0 };
						itoa(c, 16, str);
						TextIOOutputStringGlobal(str);
						break;
					}
					default:
						TextIOOutputStringGlobal("?");
						break;
				}
				break;
			default:
display:
				TextIOOutputCharacter(input[i], gCursorX, gCursorY, gColor, gBackdrop, gRenderBackdrop);
				gCursorX += 8;
				break;
		}
		
	}
}

void PrintT(char input[], ...) {
	va_list		args;
	va_start(args, input);
	
	TextIOOutputFormatedString(input, __strlen(input), args);
	
	va_end(args);
}

void TextIOTest(UINT64 mode) {
	if (!TextIOIsInitialized())
		return;

	if (mode == 0) {
		
		TextIOOutputCharacter('!', 20, 20, 0xFF000000, 0, 0);
		TextIOOutputCharacter('~', 28, 20, 0xFFFFFFFF, 0, 1);
		TextIOOutputCharacter('}', 36, 20, 0xFF000000, 0, 0);

		TextIOOutputString("test of the new ............................................................................................................................................\nhhh", 500, 20, 0xFFBFBFBF, 0, 1, 0, 550, 0, height);
	
		TextIOOutputString("Alignment test\n", 20, 100, 0xFFbFbfbF, 0, 1, 0, width, 0, height);
		PrintT("test %i 0x%X %x %x %c '%s' %i '%s'\n",185,0x666666,0x12345678,0xabcdef00, '*', "g", 0x69LL, "mn");
	
	}
}

void drawMap() {
	int x = 0;
	int y = 0;

	TextIOGetCursorPosition(&x, &y);

	x = 0;
	y += 10;

	for (int a = 0; a < GlobalPhysicalMemoryMapSize; a++) {
		if (GlobalPhysicalMemoryMap[a] == 1)
			framebuffer[x + y * width] = 0xFF007F00;
		else if (GlobalPhysicalMemoryMap[a] == 2)
			framebuffer[x + y * width] = 0xFFAFAF00;
		else
			framebuffer[x + y * width] = 0xFF7F0000;
		x++;
		if (x > width)
		{
			y++;
			x = 0;
		}
	}
}