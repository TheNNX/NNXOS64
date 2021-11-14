#include "memory/nnxalloc.h"

void* operator new(size_t a)
{
	void* result = NNXAllocatorAlloc(a);
	return result;
}

void operator delete(void* a)
{
	NNXAllocatorFree(a);
}

void* operator new[](size_t a)
{
	void* result = NNXAllocatorAlloc(a);
	return result;
}

void operator delete(void* a, size_t unused_b)
{
	NNXAllocatorFree(a);
}

/* TODO: If some memory managment code doesn't work, check this code */
void operator delete[](void* a)
{
	NNXAllocatorFree(a);
}