#include "MemoryOperations.h"

void MemSet(void* dest, UINT8 value, UINT64 c) {
	for (int b = 0; b < c; b++) {
		*((UINT8*)dest) = value;
		dest = ((UINT64)dest) + 1;
	}
}

void MemCopy(void* dst, void *src, UINT64 size) {
	for (int b = 0; b < size; b++) {
		((UINT8*)dst)[b] = ((UINT8*)src)[b];
	}
}