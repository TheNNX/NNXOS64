#include "nnxint.h"
#include "video/SimpleTextIO.h"
#include "memory/paging.h"

void main(int* framebuffer, int* framebufferEnd, UINT32 width, UINT32 height, void (*ExitBootServices)(void*, UINT64), void* imageHandle, UINT64 n, UINT8* nnxMMap, UINT64 nnxMMapSize) {
	ExitBootServices(imageHandle, n);
	TextIOInitialize(framebuffer, framebufferEnd, width, height);
	
	int boundingBox[4] = {0,width,0,height};
	TextIOSetBoundingBox(boundingBox);

	TextIOSetCursorPosition(0, 0);
	TextIOClear();

	PrintT("NNXOSLDR.EXE\n");
	PrintT("Stage 2 loaded...\n");

	PagingInit();
	PagingTest();

	PrintT("Memory map: ");
	TextIOSetColorInformation(0xffffffff, 0xff007f00, 1);
	PrintT(" FREE ");
	TextIOSetColorInformation(0xffffffff, 0xff7f0000, 1);
	PrintT(" USED ");
	TextIOSetColorInformation(0xffffffff, 0, 1);
	
	int x = 0;
	int y = 0;

	TextIOGetCursorPosition(&x, &y);

	x = 0;
	y += 10;

	for (int a = 0; a < nnxMMapSize; a++) {
		if (nnxMMap[a])
			framebuffer[x + y * width] = 0xFF007F00;
		else
			framebuffer[x + y * width] = 0xFF7F0000;
		x++;
		if (x > width)
		{
			y++;
			x = 0;
		}
	}

	while (1);
}