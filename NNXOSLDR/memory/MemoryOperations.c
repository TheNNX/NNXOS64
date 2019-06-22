#include "MemoryOperations.h"

void memset(void* dest, UINT64 value, UINT64 c) {
	for (int b = 0; b < c; b++) {
		*((UINT8*)dest) = value;
		dest = ((UINT64)dest) + 1;
	}
}