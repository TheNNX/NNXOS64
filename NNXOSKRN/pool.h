/*
	For now just a wrapper for NNXAllocator
	(to be changed)
*/

#ifndef NNX_POOL_HEADER
#define NNX_POOL_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum _POOL_TYPE
	{
		NonPagedPool = 0,
		PagedPool = 1
	} POOL_TYPE;

	PVOID ExAllocatePool(POOL_TYPE type, SIZE_T size);
	PVOID ExAllocatePoolWithTag(POOL_TYPE type, SIZE_T size, ULONG tag);
	PVOID ExAllocatePoolZero(POOL_TYPE type, SIZE_T size, ULONG tag);
	PVOID ExAllocatePoolUninitialized(POOL_TYPE type, SIZE_T size, ULONG tag);
	VOID ExFreePoolWithTag(PVOID data, ULONG tag);
	VOID ExFreePool(PVOID);

#define POOL_TAG_FROM_STR(x) (*((ULONG*)x))

#ifdef __cplusplus
}
#endif

#endif
