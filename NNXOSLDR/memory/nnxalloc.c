#include "nnxalloc.h"
#include "../video/SimpleTextIo.h"
#include "MemoryOperations.h"
#include "text.h"

MemoryBlock* first;
BOOL dirty = FALSE;
BOOL noMerge = FALSE;

#ifdef DEBUG

UINT32 minX = 0;
UINT32 maxX = 0;
UINT32 minY = 0;
UINT32 maxY = 0;

extern UINT32* gFramebuffer;
extern UINT32 gPixelsPerScanline;
extern UINT32 gWidth;

UINT32 lastX = 0;

UINT32 times = 0, required = 100;
const UINT32 progressBarLength = 160;

#endif

void ForceScreenUpdate()
{
#ifdef DEBUG
	const UINT32 controlColor = 0xFFC3C3C3, controlBckgrd = 0xFF606060;
	const UINT32 usedColor = 0xFF800000, freeColor = 0xFF008000;
	const UINT32 pad = 10;
	UINT32* cFramebuffer = gFramebuffer + minX;
	UINT32 oldBoundingBox[4], newBoundingBox[4] = { minX, maxX, minY, maxY };
	UINT32 cursorX, cursorY, oldColor, oldBackground, oldRenderBack, currentPosX, currentPosY, i;
	UINT32 pixelsUsed = 0;

	for (UINT32 y = minY; y < maxY; y++)
	{
		cFramebuffer += gPixelsPerScanline;
		for (UINT32 x = minX; x < lastX; x++)
		{
			cFramebuffer[x] = controlColor;
		}
	}

	TextIoGetCursorPosition(&cursorX, &cursorY);
	TextIoGetBoundingBox(oldBoundingBox);
	TextIoSetBoundingBox(newBoundingBox);
	TextIoGetColorInformation(&oldColor, &oldBackground, &oldRenderBack);

	TextIoSetCursorPosition(minX + 8, minY + 6);
	TextIoSetColorInformation(0xFF000000, controlColor, 0);

	if (first)
	{
		PrintT("Allocator usage: %i%%, total: %iKiB", (NNXAllocatorGetUsedMemory() * 100) / NNXAllocatorGetTotalMemory(), (NNXAllocatorGetTotalMemory() + 1) / 1024);

		TextIoGetCursorPosition(&currentPosX, &currentPosY);

		cFramebuffer = gFramebuffer + (currentPosX + pad) + currentPosY * gPixelsPerScanline;

		for (i = 0; i < progressBarLength; i++)
		{
			gFramebuffer[gPixelsPerScanline * (currentPosY - 1) + currentPosX + 10 + i] = controlBckgrd;
			gFramebuffer[gPixelsPerScanline * (currentPosY + 8) + currentPosX + 10 + i] = controlBckgrd;
		}

		pixelsUsed = (NNXAllocatorGetUsedMemory() * progressBarLength) / NNXAllocatorGetTotalMemory();

		for (i = 0; i < 8; i++)
		{
			UINT32 j;
			gFramebuffer[gPixelsPerScanline * (currentPosY + i) + currentPosX + pad - 1] = controlBckgrd;
			gFramebuffer[gPixelsPerScanline * (currentPosY + i) + currentPosX + pad + progressBarLength] = controlBckgrd;
			for (j = 0; j < progressBarLength; j++)
			{
				cFramebuffer[j] = (j < pixelsUsed) ? usedColor : freeColor;
			}
			cFramebuffer += gPixelsPerScanline;
		}

		lastX = currentPosX + pad + progressBarLength + 1;
	}
	TextIoSetColorInformation(oldColor, oldBackground, oldRenderBack);
	TextIoSetCursorPosition(cursorX, cursorY);
	TextIoSetBoundingBox(oldBoundingBox);
#endif
}

void UpdateScreen()
{
#ifdef DEBUG
	if ((times % required) == 0)
	{
		times = 0;
		ForceScreenUpdate();
	}
	times++;
#endif
}

void InitializeScreen()
{
#ifdef DEBUG
	minX = 0;
	maxX = gWidth;
	maxY = 19;
	minY = 0;
	times = 0;
	lastX = maxX;
#endif
}

void NNXAllocatorInitialize()
{
	first = 0;
	InitializeScreen();
}

UINT64 CountBlockSize(UINT8 flags)
{
	UINT64 result = 0;

	MemoryBlock* current = first;
	while (current)
	{
		if (current->flags == flags || (flags & 0x80))
			result += current->size + ((flags & 0x80) ? sizeof(MemoryBlock) : 0);
		current = current->next;
	}

	return result;
}

UINT64 NNXAllocatorGetTotalMemory()
{
	return CountBlockSize(0xFF);
}


UINT64 NNXAllocatorGetUsedMemory()
{
	return NNXAllocatorGetTotalMemory() - CountBlockSize(MEMBLOCK_FREE);
}

UINT64 NNXAllocatorGetUsedMemoryInBlocks()
{
	return CountBlockSize(MEMBLOCK_USED);
}

UINT64 NNXAllocatorGetFreeMemory()
{
	return CountBlockSize(MEMBLOCK_FREE);
}

void NNXAllocatorAppend(void* memblock, UINT64 memblockSize)
{
	if (!first)
	{
		first = memblock;
		first->size = memblockSize - sizeof(MemoryBlock) - 1;
		first->next = 0;
		first->flags = MEMBLOCK_FREE;
	}
	else
	{
		MemoryBlock* current = first;
		UINT64 i = 0;
		while (current->next)
		{
			i++;
			current = current->next;
		}

		current->next = memblock;
		current->next->size = memblockSize - sizeof(MemoryBlock);
		current->next->next = 0;
		current->next->flags = MEMBLOCK_FREE;
	}
	dirty = true;
	UpdateScreen();
}

void TryMerge()
{
	if (noMerge == TRUE)
	{
		PrintT("Merge failed. Blocks:\n");
		MemoryBlock* current = first;
		while (current)
		{
			if (current->size > 256)
				PrintT("Block %i %x %x %x\n", current->size, current, current->next, current->flags);
			current = current->next;
		}
		while (1);
	}
	PrintT("System memory allocation utility is attempting to merge free memory blocks (%i dirty)\n", dirty);
	MemoryBlock* current = first;

	if (current == 0)
		return 0;

	dirty = FALSE;

	while (current->next)
	{
		if (!(current->flags & MEMBLOCK_USED) && !(current->next->flags & MEMBLOCK_USED) &&
			(((UINT64) current) + current->size + sizeof(MemoryBlock)) == current->next)
		{
			current->size += current->next->size + sizeof(MemoryBlock);
			current->next = current->next->next;
		}
		else
		{
			current = current->next;
		}

	}

	UpdateScreen();
}

void* NNXAllocatorAlloc(UINT64 size)
{
	MemoryBlock* current = first;

	while (current)
	{

		if (current->flags & (~MEMBLOCK_USED))
		{
			PrintT("Invalid block of size %x at address %x leading to address %x (flags %b) in kernel allocator\n", current->size, current, current->next, current->flags);
			while (1);
		}

		if (!(current->flags & MEMBLOCK_USED))
		{

			if (current->size == size)
			{
				current->flags |= MEMBLOCK_USED;
				dirty = TRUE;
				UpdateScreen();
				return (void*) (((UINT64) current) + sizeof(MemoryBlock));
			}
			else if (current->size > size + sizeof(MemoryBlock))
			{
				current->flags |= MEMBLOCK_USED;
				MemoryBlock* newBlock = (MemoryBlock*) ((((UINT64) current) + size + sizeof(MemoryBlock)));
				newBlock->next = current->next;
				current->next = newBlock;
				newBlock->size = current->size - (size + sizeof(MemoryBlock));
				current->size = size;
				newBlock->flags = MEMBLOCK_FREE;
				dirty = TRUE;
				UpdateScreen();
				return (void*) (((UINT64) current) + sizeof(MemoryBlock));
			}
		}
		current = current->next;
	}

	if (dirty)
	{
		TryMerge();
		noMerge = TRUE;
		UpdateScreen();
		return NNXAllocatorAlloc(size);
	}
	PrintT("No memory block of size %iB found, system halted.\n", size);
	while (1);
}

void* NNXAllocatorAllocArray(UINT64 n, UINT64 size)
{
	void* result = NNXAllocatorAlloc(n*size);
	if (result)
		MemSet(result, 0, size*n);
	return result;
}

void NNXAllocatorFree(void* address)
{
	dirty = true;
	MemoryBlock* toBeFreed = ((UINT64) address - sizeof(MemoryBlock));
	toBeFreed->flags &= (~MEMBLOCK_USED);
	UpdateScreen();
}

VOID NNXAllocatorF()
{
	PrintT("Total memory: %i\nUsed memory: %i\nFree memory: %i\nFirst memory block: 0x%X\n", NNXAllocatorGetTotalMemory(), NNXAllocatorGetUsedMemory(), NNXAllocatorGetFreeMemory(), first);
}