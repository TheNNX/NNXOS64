#include "nnxint.h"
#include "video/SimpleTextIO.h"
#include "memory/paging.h"

void main(int* framebuffer, int* framebufferEnd, UINT32 width, UINT32 height, void (*ExitBootServices)(void*, UINT64), void* imageHandle, UINT64 n) {
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

	while (1);
}