#ifndef NNX_POOL_HEADER
#define NNX_POOL_HEADER

#include <ntlist.h>
#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum _POOL_TYPE
	{
		NonPagedPool = 0,
		PagedPool = 1
	} POOL_TYPE;

	typedef struct _POOL_HEADER
	{
		LIST_ENTRY PoolEntry;
		/* any non-zero value indicates an used block */
		DWORD AllocationTag;
		POOL_TYPE PoolType;
		SIZE_T Size;
	}POOL_HEADER, * PPOOL_HEADER;

	typedef struct _POOL_DESCRIPTOR
	{
		LIST_ENTRY PoolHead;
		KSPIN_LOCK PoolLock;
		KIRQL MaximalIrql;
		PVOID AddressStart;
		PVOID AddressEnd;
	}POOL_DESCRIPTOR, * PPOOL_DESCRIPTOR;

	BOOL
	ExVerifyPoolBlock(
		PPOOL_HEADER blockHeader,
		POOL_TYPE poolType
	);

	VOID
	ExInitEmptyPoolBlock(
		PPOOL_HEADER pPoolBlock,
		SIZE_T Size,
		POOL_TYPE PoolType
	);

	BOOL
	ExExpandPool(
		POOL_TYPE poolType,
		PVOID pMemory,
		SIZE_T memoryBlockSize
	);

	PVOID
	ExAllocatePool(
		POOL_TYPE type,
		SIZE_T size
	);

	PVOID
	ExAllocatePoolZero(
		POOL_TYPE type,
		SIZE_T size,
		ULONG tag
	);

	PVOID
	ExAllocatePoolUninitialized(
		POOL_TYPE type,
		SIZE_T size,
		ULONG tag
	);

	NTSTATUS
	ExInitializePool(
		PVOID PagedPoolMemoryRegion,
		SIZE_T PagedPoolMemoryRegionSize,
		PVOID NonPagedPoolMemoryRegion,
		SIZE_T NonPagedPoolMemoryRegionSize
	);

	NTSTATUS
	ExPoolSelfCheck();

	VOID
	ExFreePool(
		PVOID data
	);

	PVOID
	ExAllocatePoolWithTag(
		POOL_TYPE type,
		SIZE_T size,
		ULONG tag
	);

	VOID
	ExFreePoolWithTag(
		PVOID data,
		ULONG tag
	);

	BOOL
	ExVerifyPool(
		POOL_TYPE type
	);

#ifdef __cplusplus
}
#endif

#endif
