#include "nnxint.h"
#include "video/SimpleTextIO.h"
#include "memory/physical_allocator.h"
#include "memory/paging.h"

const char version[] = " 0.1";

void main(int* framebuffer, int* framebufferEnd, UINT32 width, UINT32 height, void (*ExitBootServices)(void*, UINT64), void* imageHandle, UINT64 n, UINT8* nnxMMap, UINT64 nnxMMapSize, UINT64 memorySize) {
	
	ExitBootServices(imageHandle, n);

	GlobalPhysicalMemoryMap = nnxMMap;
	GlobalPhysicalMemoryMapSize = nnxMMapSize;

	for (int a = 128; a < 384; a++) {
		GlobalPhysicalMemoryMap[a] = 0;
	}

	MemorySize = memorySize;

	TextIOInitialize(framebuffer, framebufferEnd, width, height);
	TextIOClear();

	PrintT("Initializing memory");
	PagingInit();
	PrintT("NNXOSLDR.exe version %s\n",version);
	PrintT("Stage 2 loaded... %x %x %i\n", framebuffer, framebufferEnd, (((UINT64)framebufferEnd) - ((UINT64)framebuffer)) / 4096);

	PrintT("Memory map: ");
	TextIOSetColorInformation(0xffffffff, 0xff007f00, 1);
	PrintT(" FREE ");
	TextIOSetColorInformation(0xffffffff, 0xff7f0000, 1);
	PrintT(" USED ");
	TextIOSetColorInformation(0xff000000, 0xffAfAf00, 1);
	PrintT(" UTIL ");
	TextIOSetColorInformation(0xffffffff, 0, 1);

	drawMap();

	while (1);
}
