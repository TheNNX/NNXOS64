#include <pool.h>
#include <MemoryOperations.h>
#define NNX_ALLOC_DEBUG 0
#include <nnxalloc.h>
#include <bugcheck.h>

PVOID ExAllocatePool(POOL_TYPE type, SIZE_T size)
{
	return ExAllocatePoolWithTag(type, size, 0);
}

PVOID ExAllocatePoolWithTag(POOL_TYPE type, SIZE_T size, ULONG tag)
{
	if (tag != 0)
	{
		/* TODO: set tag */
	}

	if (type != NonPagedPool)
		KeBugCheck(HAL_MEMORY_ALLOCATION);

	/* TODO: care about pool type */
	return NNXAllocatorAlloc(size);
}

PVOID ExAllocatePoolZero(POOL_TYPE type, SIZE_T size, ULONG tag)
{
	PVOID result = ExAllocatePoolWithTag(type, size, tag);

	if (result == NULL)
		return NULL;

	MemSet(result, 0, size);
	return result;
}

PVOID ExAllocatePoolUninitialized(POOL_TYPE type, SIZE_T size, ULONG tag)
{
	return ExAllocatePoolWithTag(type, size, tag);
}

VOID ExFreePoolWithTag(PVOID data, ULONG tag)
{
	if (tag != 0)
	{
		/* TODO: check tag */
	}
	
	NNXAllocatorFree(data);
}

VOID ExFreePool(PVOID data)
{
	ExFreePoolWithTag(data, 0);
}