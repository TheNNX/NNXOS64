#include <pool.h>

void* operator new(size_t Size)
{
	void* result = ExAllocatePool(NonPagedPool, Size);
	return result;
}

void operator delete(void* Address)
{
	ExFreePool(Address);
}

void* operator new[](size_t Size)
{
	void* result = ExAllocatePool(NonPagedPool, Size);
	return result;
}

void operator delete(void* a, size_t unused_b)
{
	ExFreePool(a);
}

/* TODO: If some memory managment code doesn't work, check this code */
void operator delete[](void* a)
{
	ExFreePool(a);
}