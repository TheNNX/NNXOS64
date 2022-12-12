/*
 * pool.c - simple NT-like pool manager implementation
 * Author: Marcin Jab³oñski
 * License: GNU LGPL v3
 *
 * Implementation details:
 * The pool manager keeps track of regisered pools by keeping pointers
 * to their POOL_DESCRIPTOR structures.
*/

#pragma intrinsic(_ReturnAddress)
#include <intrin.h>
#include <pool.h>
#include <bugcheck.h>
#include <HAL/spinlock.h>
#include <rtl/rtl.h>

#define POOL_ALLOCATION_TAG_FREE 0
#ifdef _M_AMD64
#define SYSTEM_ALIGNMENT 16
#elif defined _M_I386
#define SYSTEM_ALIGNMENT 8
#else
#error Undefined pool alignment
#endif

/* pointers to the pool descriptor structures */
static PPOOL_DESCRIPTOR Pools[16] = { 0 };

static
VOID
InsertAfter(
	PPOOL_HEADER oldHeader,
	PPOOL_HEADER newHeader
);

static
BOOL
ValidateTag(
	const DWORD Tag
);

static
VOID
DebugEnumeratePoolBlocks(
	POOL_TYPE poolType
);

VOID
ExMergePool(
	POOL_TYPE poolType
);

VOID 
ExInitEmptyPoolBlock(
	PPOOL_HEADER pPoolBlock,
	SIZE_T Size,
	POOL_TYPE PoolType
)
{
	pPoolBlock->AllocationTag = POOL_ALLOCATION_TAG_FREE;
	pPoolBlock->Size = Size;
	pPoolBlock->PoolType = PoolType;
}

BOOL
/* @brief ExExpandPool - adds a memory block to the tail of block list
 * and initializes it as an empty block. 
 * @return FALSE on failure, TRUE otherwise */
ExExpandPool(
	POOL_TYPE poolType,
	PVOID pMemory,
	SIZE_T memoryBlockSize
)
{
	PPOOL_DESCRIPTOR poolDecsriptor;
	PPOOL_HEADER pBlock;
	
	poolDecsriptor = Pools[poolType];

	/* no such pool registered */
	if (poolDecsriptor == NULL)
	{
		return FALSE;
	}

	pBlock = (PPOOL_HEADER)pMemory;

	/* initialize the block */
	ExInitEmptyPoolBlock(
		pBlock,
		memoryBlockSize - sizeof(POOL_HEADER),
		poolType
	);

	/* and add it to the pool's linked list */
	ExInterlockedInsertTailList(
		&poolDecsriptor->PoolHead,
		&pBlock->PoolEntry, 
		&poolDecsriptor->PoolLock
	);

	return TRUE;
}

PVOID 
ExAllocatePool(
	POOL_TYPE type, 
	SIZE_T size
)
{
	return ExAllocatePoolWithTag(
		type,
		size, 
		'LOOP' /* POOL backwards */
	);
}

PVOID
ExAllocatePoolZero(
	POOL_TYPE type, 
	SIZE_T size, 
	ULONG tag
)
{
	PVOID result;
	
	result = ExAllocatePoolWithTag(
		type, 
		size, 
		tag
	);

	if (result == NULL)
		return NULL;

	RtlZeroMemory(
		result, 
		size
	);

	return result;
}

PVOID 
ExAllocatePoolUninitialized(
	POOL_TYPE type, 
	SIZE_T size, 
	ULONG tag
)
{
	return ExAllocatePoolWithTag(
		type, 
		size, 
		tag
	);
}

VOID
ExFreePool(
	PVOID data
)
{
	ExFreePoolWithTag(data, 0);
}

static
VOID
InsertAfter(
	PPOOL_HEADER oldHeader,
	PPOOL_HEADER newHeader
)
{
	newHeader->PoolEntry.Prev = &oldHeader->PoolEntry;
	newHeader->PoolEntry.Next = oldHeader->PoolEntry.Next;

	oldHeader->PoolEntry.Next->Prev = &newHeader->PoolEntry;
	oldHeader->PoolEntry.Next = &newHeader->PoolEntry;
}

static
BOOL
ValidateTag(
	const DWORD Tag
)
{
	const UBYTE* pTagAsByteArray;
	INT i;

	pTagAsByteArray = (const UBYTE*)&Tag;
	
	for (i = 0; i < 4; i++)
	{
		if (pTagAsByteArray[i] < ' ' || pTagAsByteArray[i] > 0x7E)
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOL
ExVerifyPool(
	POOL_TYPE type
)
{
	PPOOL_DESCRIPTOR poolDescriptor;
	PPOOL_HEADER currentCheckedBlock;
	KIRQL irql;

	poolDescriptor = Pools[type];
	KeAcquireSpinLock(&poolDescriptor->PoolLock, &irql);

	DebugEnumeratePoolBlocks(type);

	/* enumerate the pool blocks */
	currentCheckedBlock = (PPOOL_HEADER)poolDescriptor->PoolHead.First;
	while ((PLIST_ENTRY)currentCheckedBlock != &poolDescriptor->PoolHead)
	{
		if (!ExVerifyPoolBlock(currentCheckedBlock, type))
		{
			PrintT("Corrupted pool block %X\n", currentCheckedBlock);
			KeReleaseSpinLock(&poolDescriptor->PoolLock, irql);
			return FALSE;
		}

		currentCheckedBlock = (PPOOL_HEADER)currentCheckedBlock->PoolEntry.Next;
	}
	KeReleaseSpinLock(&poolDescriptor->PoolLock, irql);

	return TRUE;
}

PVOID
ExAllocatePoolWithTag(
	POOL_TYPE type,
	SIZE_T size,
	ULONG tag
)
{
	PPOOL_DESCRIPTOR poolDescriptor;
	PPOOL_HEADER currentCheckedBlock;
	SIZE_T sizeWithBlockHeader;
	SIZE_T alignment;
	KIRQL irql;

	/* caluclate the desired alignment */
	if (size < PAGE_SIZE)
		alignment = SYSTEM_ALIGNMENT;
	else
		alignment = PAGE_SIZE;

	
	if (tag == 0)
	{
		KeBugCheckEx(
			BAD_POOL_CALLER,
			(ULONG_PTR)0x9B,
			(ULONG_PTR)type,
			(ULONG_PTR)size,
			(ULONG_PTR)_ReturnAddress()
		);
	}

	if (!ValidateTag(tag))
	{
		KeBugCheckEx(
			BAD_POOL_CALLER,
			(ULONG_PTR)0x9D,
			(ULONG_PTR)tag,
			(ULONG_PTR)type,
			(ULONG_PTR)_ReturnAddress()
		);
	}

	/* get the pool descriptor */
	poolDescriptor = Pools[type];

	irql = KeGetCurrentIrql();
	
	if (irql > poolDescriptor->MaximalIrql)
	{
		PrintT("Call to %s with IRQL %i failed due to required IRQL being %i\n", __FUNCTION__, irql, poolDescriptor->MaximalIrql);
		KeBugCheckEx(
			BAD_POOL_CALLER,
			0x08,
			irql,
			type,
			size
		);
	}

	/* no such pool, return */
	if (poolDescriptor == NULL)
		return NULL;

	/* lock the pool */
	KeAcquireSpinLock(&poolDescriptor->PoolLock, &irql);

	/* calculate the total size of the allocation */
	sizeWithBlockHeader = size + sizeof(POOL_HEADER);

	/* enumerate the pool blocks */
	currentCheckedBlock = (PPOOL_HEADER)poolDescriptor->PoolHead.First;
	while ((PLIST_ENTRY)currentCheckedBlock != &poolDescriptor->PoolHead)
	{
		if (!ExVerifyPoolBlock(currentCheckedBlock, type))
		{
			DebugEnumeratePoolBlocks(type);
			KeBugCheckEx(
				BAD_POOL_CALLER,
				0x01,
				(ULONG_PTR)currentCheckedBlock,
				(ULONG_PTR)currentCheckedBlock->PoolEntry.Next,
				0
			);
		}

		/* if the current block is free */
		if (currentCheckedBlock->AllocationTag == POOL_ALLOCATION_TAG_FREE && size < currentCheckedBlock->Size)
		{
			BOOL isAllocationPossible;
			/* pointer to the first byte of memory of the block */
			ULONG_PTR blockMemoryPtr;
			/* pointer to the first byte after the memory of the block */
			ULONG_PTR blockMemoryEndPtr;
			/* first possible location for the data region of the block to start
			 * with the given alignment in mind */
			ULONG_PTR potentialDataPtr;
			/* number of bytes preceding the data if the block was to be used */
			LONG_PTR precedingBytes;
			/* number of bytes following the data if the block was to be used */
			LONG_PTR followingBytes;
			/* pointer to the first byte after the data, if the block was to be allocated */
			ULONG_PTR potentialDataEndPtr;

			blockMemoryPtr = sizeof(POOL_HEADER) + (ULONG_PTR)currentCheckedBlock;
			blockMemoryEndPtr = blockMemoryPtr + currentCheckedBlock->Size;

			/* align: round up to next possible starting address for the data region of the block */
			potentialDataPtr = ((blockMemoryPtr + alignment - 1) / alignment) * alignment;
			
			isAllocationPossible = FALSE;

			do
			{
				isAllocationPossible = TRUE;

				potentialDataEndPtr = potentialDataPtr + size;
				precedingBytes = potentialDataPtr - blockMemoryPtr;
				followingBytes = blockMemoryEndPtr - potentialDataEndPtr;

				/* if a new block has to be created out of the data preceeding, check if there's enough room
				 * to store a new block header */
				if (precedingBytes != 0 && precedingBytes < (LONG_PTR)sizeof(POOL_HEADER))
				{
					isAllocationPossible = FALSE;
				}

				/* the same as above, but for following bytes */
				if (followingBytes != 0 && followingBytes < (LONG_PTR)sizeof(POOL_HEADER))
				{
					isAllocationPossible = FALSE;
				}

				/* Initial state of the block (the block found in the loop):
				 * [header][                      n bytes of data                   ]
				 *
				 * It is possible, that due to alignment a split is neccesary:
				 * [header][    m bytes of data   |     (n - m) bytes of data       ]
				 * Where m is the precedingBytes
				 *
				 * If m != 0, a new header is created:
				 * [oldHdr][ m - h bytes ][newHdr][     (n - m) bytes of data       ]
				 * Where h is sizeof(POOL_HEADER)
				 *
				 * oldHdr and newHdr pool entries are linked.
				 *
				 *
				 * If m == 0, curHdr = oldHdr,
				 * else curHdr = newHdr.
				 *
				 *
				 * It is also possible that (n - m) > size of the desired allocation
				 * [curHdr][     (n - m) bytes of data       ]
				 * [curHdr][size bytes][       f bytes       ]
				 * Where f is followingBytes
				 *
				 * Then, yet another header is created.
				 * [curHdr][size bytes][folHdr][ f - h bytes ]
				 * Where h is sizeof(POOL_HEADER)
				 *
				 * curHdr and folHdr pool entries are linked.
				 *
				 */
				if (isAllocationPossible)
				{
					/* create new headers if neccessary */
					if (precedingBytes != 0)
					{
						/* newHdr */
						PPOOL_HEADER newHeader;
						/* oldHdr */
						PPOOL_HEADER oldHeader;

						oldHeader = (PPOOL_HEADER)currentCheckedBlock;
						newHeader = (PPOOL_HEADER)(potentialDataPtr - sizeof(POOL_HEADER));

						/* link the headers */
						InsertAfter(oldHeader, newHeader);

						oldHeader->Size = precedingBytes - sizeof(POOL_HEADER);

						newHeader->PoolType = type;

						/* curHdr = newHdr */
						currentCheckedBlock = newHeader;
					}

					if (followingBytes != 0)
					{
						/* folHdr */
						PPOOL_HEADER followingHeader;

						followingHeader = (PPOOL_HEADER)potentialDataEndPtr;

						/* link the headers */
						InsertAfter(currentCheckedBlock, followingHeader);
						followingHeader->Size = followingBytes - sizeof(POOL_HEADER);
						followingHeader->AllocationTag = 0;
						followingHeader->PoolType = type;
					}

					/* the excess memory (if any) was allocated to new blocks created above */
					currentCheckedBlock->Size = size;
					/* set the desired tag (this also marks the block non-free) */
					currentCheckedBlock->AllocationTag = tag;

					KeReleaseSpinLock(&poolDescriptor->PoolLock, irql);
					return (PVOID)potentialDataPtr;
				}

				/* try the next aligned location */
				potentialDataPtr += alignment;
			}
			while (isAllocationPossible == FALSE && potentialDataEndPtr < blockMemoryEndPtr);
		}

		/* go to the next pool block */
		currentCheckedBlock = (PPOOL_HEADER)currentCheckedBlock->PoolEntry.Next;
	}

	PrintT("ExAllocatePool failed, size=%x pooltype=%i\n", size, (ULONG_PTR)(type));
	DebugEnumeratePoolBlocks(type);
	KeReleaseSpinLock(&poolDescriptor->PoolLock, irql);
	return NULL;
}

VOID
ExFreePoolWithTag(
	PVOID data,
	ULONG tag
)
{
	PPOOL_DESCRIPTOR pPoolDescriptor;
	PPOOL_HEADER pPoolHeader;
	POOL_TYPE type;
	KIRQL irql;

	pPoolHeader = (PPOOL_HEADER)((ULONG_PTR)data - sizeof(POOL_HEADER));
	type = pPoolHeader->PoolType;

	if (pPoolHeader->AllocationTag == POOL_ALLOCATION_TAG_FREE)
	{
		switch (pPoolHeader->PoolType)
		{
		default:
		case PagedPool:
			KeBugCheckEx(
				BAD_POOL_CALLER,
				0x48,
				(ULONG_PTR)data,
				0,
				0
			);
		case NonPagedPool:
			KeBugCheckEx(
				BAD_POOL_CALLER,
				0x44,
				(ULONG_PTR)data,
				0, 
				0
			);
		}
	}

	if (tag != pPoolHeader->AllocationTag && tag != POOL_ALLOCATION_TAG_FREE)
	{
		KeBugCheckEx(
			BAD_POOL_CALLER, 
			0x0A,
			(ULONG_PTR)Pools[type],
			(ULONG_PTR)pPoolHeader->AllocationTag,
			(ULONG_PTR)tag
		);
	}

	irql = KeGetCurrentIrql();
	pPoolDescriptor = Pools[type];
	if (irql > pPoolDescriptor->MaximalIrql)
	{
		KeBugCheckEx(
			BAD_POOL_CALLER,
			0x09,
			(ULONG_PTR)irql,
			(ULONG_PTR)type,
			(ULONG_PTR)pPoolDescriptor
		);
	}

	KeAcquireSpinLock(&pPoolDescriptor->PoolLock, &irql);
	pPoolHeader->AllocationTag = 0;
	KeReleaseSpinLock(&pPoolDescriptor->PoolLock, irql);

	ExMergePool(type);
}

static
VOID
MergeWithNextBlock(
	PPOOL_HEADER block
)
{
	PPOOL_HEADER next;
	PPOOL_DESCRIPTOR desciptor;
	
	desciptor = Pools[block->PoolType];

	if (desciptor->PoolLock == 0)
	{
		KeBugCheckEx(
			SPIN_LOCK_NOT_OWNED,
			__LINE__,
			0,
			0,
			0
		);
	}

	if (ExVerifyPoolBlock(block, block->PoolType) == FALSE ||
		ExVerifyPoolBlock((PPOOL_HEADER)block->PoolEntry.Next, block->PoolType) == FALSE)
	{
		KeBugCheckEx(
			BAD_POOL_HEADER,
			0, /* TODO, fill those params */
			0,
			0,
			0
		);
	}

	next = (PPOOL_HEADER)block->PoolEntry.Next;
	block->Size = block->Size + next->Size + sizeof(POOL_HEADER);
	next->PoolEntry.Next->Prev = &block->PoolEntry;
	block->PoolEntry.Next = next->PoolEntry.Next;
}

VOID
ExMergePool(
	POOL_TYPE poolType
)
{
	PPOOL_HEADER currentHeader;
	KIRQL irql;

	KeAcquireSpinLock(&Pools[poolType]->PoolLock, &irql);
	currentHeader = (PPOOL_HEADER)Pools[poolType]->PoolHead.First;

	while (&currentHeader->PoolEntry != &Pools[poolType]->PoolHead)
	{
		if (!ExVerifyPoolBlock(currentHeader, poolType))
		{
			KeBugCheckEx(
				BAD_POOL_CALLER,
				0x01,
				(ULONG_PTR)currentHeader,
				(ULONG_PTR)currentHeader->PoolEntry.Next,
				0
			);
		}

		if (currentHeader->Size + (ULONG_PTR)currentHeader + sizeof(POOL_HEADER) == (ULONG_PTR)currentHeader->PoolEntry.Next &&
			currentHeader->AllocationTag == POOL_ALLOCATION_TAG_FREE &&
			((PPOOL_HEADER)currentHeader->PoolEntry.Next)->AllocationTag == POOL_ALLOCATION_TAG_FREE)
		{
			MergeWithNextBlock(currentHeader);
		}
		else
		{
			currentHeader = (PPOOL_HEADER)currentHeader->PoolEntry.Next;
		}
	}

	KeReleaseSpinLock(&Pools[poolType]->PoolLock, irql);
}

BOOL
ExVerifyPoolBlock(
	PPOOL_HEADER blockHeader,
	POOL_TYPE poolType
)
{
	BOOL result;
	PPOOL_DESCRIPTOR poolDecsriptor = Pools[poolType];

	result = TRUE;
	
	if (poolDecsriptor->PoolLock == 0)
		KeBugCheckEx(SPIN_LOCK_NOT_OWNED, __LINE__, 0, 0, 0);

	/* check if pool type remains unchanged */
	if (blockHeader->PoolType != poolType)
	{
		result = FALSE;
	}

	/* check the block address */
	if (result &&
		(PVOID)blockHeader >= poolDecsriptor->AddressEnd ||
		(PVOID)blockHeader < poolDecsriptor->AddressStart)
	{
		result = FALSE;
	}

	/* check the list entries */
	if (result &&
		(PVOID)blockHeader->PoolEntry.Next != &poolDecsriptor->PoolHead)
	{
		if ((PVOID)blockHeader->PoolEntry.Next >= poolDecsriptor->AddressEnd ||
			(PVOID)blockHeader->PoolEntry.Next < poolDecsriptor->AddressStart)
		{
			result = FALSE;
		}
	}

	if (result &&
		(PVOID)blockHeader->PoolEntry.Prev != &poolDecsriptor->PoolHead)
	{
		if ((PVOID)blockHeader->PoolEntry.Prev >= poolDecsriptor->AddressEnd ||
			(PVOID)blockHeader->PoolEntry.Prev < poolDecsriptor->AddressStart)
		{
			result = FALSE;
		}
	}

	if (blockHeader->AllocationTag != 0)
	{
		result = result && ValidateTag(blockHeader->AllocationTag);
	}

	return result;
}

NTSTATUS
ExInitializePool(
	PVOID PagedPoolMemoryRegion,
	SIZE_T PagedPoolMemoryRegionSize,
	PVOID NonPagedPoolMemoryRegion,
	SIZE_T NonPagedPoolMemoryRegionSize
)
{
	PPOOL_DESCRIPTOR pPagedPoolDesc, pNonPagedPoolDesc;
	PPOOL_HEADER pagedFirstBlock, nonPagedFirstBlock;

	if (NonPagedPoolMemoryRegionSize <= sizeof(POOL_DESCRIPTOR) ||
		PagedPoolMemoryRegionSize <= sizeof(POOL_DESCRIPTOR))
	{
		return STATUS_NO_MEMORY;
	}

	PrintT("Allocated pools %X %X\n", PagedPoolMemoryRegion, NonPagedPoolMemoryRegion);

	pPagedPoolDesc = (PPOOL_DESCRIPTOR)PagedPoolMemoryRegion;
	pNonPagedPoolDesc = (PPOOL_DESCRIPTOR)NonPagedPoolMemoryRegion;

	pPagedPoolDesc->AddressStart = (PVOID)((ULONG_PTR)PagedPoolMemoryRegion + sizeof(*pPagedPoolDesc));
	pNonPagedPoolDesc->AddressStart = (PVOID)((ULONG_PTR)NonPagedPoolMemoryRegion + sizeof(*pNonPagedPoolDesc));

	pPagedPoolDesc->AddressEnd = (PVOID)((ULONG_PTR)PagedPoolMemoryRegion + PagedPoolMemoryRegionSize);
	pNonPagedPoolDesc->AddressEnd = (PVOID)((ULONG_PTR)NonPagedPoolMemoryRegion + NonPagedPoolMemoryRegionSize);

	pPagedPoolDesc->MaximalIrql = APC_LEVEL;
	pNonPagedPoolDesc->MaximalIrql = 0xFF;

	KeInitializeSpinLock(&pPagedPoolDesc->PoolLock);
	KeInitializeSpinLock(&pNonPagedPoolDesc->PoolLock);

	Pools[NonPagedPool] = pNonPagedPoolDesc;
	Pools[PagedPool] = pPagedPoolDesc;

	pagedFirstBlock = (PPOOL_HEADER)pPagedPoolDesc->AddressStart;
	nonPagedFirstBlock = (PPOOL_HEADER)pNonPagedPoolDesc->AddressStart;

	nonPagedFirstBlock->PoolType = NonPagedPool;
	pagedFirstBlock->PoolType = PagedPool;

	pagedFirstBlock->Size = PagedPoolMemoryRegionSize - sizeof(*pPagedPoolDesc) - sizeof(POOL_HEADER);
	nonPagedFirstBlock->Size = NonPagedPoolMemoryRegionSize - sizeof(*pNonPagedPoolDesc) - sizeof(POOL_HEADER);

	PrintT("Initial non paged pool size %i\n", nonPagedFirstBlock->Size);

	pagedFirstBlock->AllocationTag = nonPagedFirstBlock->AllocationTag = POOL_ALLOCATION_TAG_FREE;

	InitializeListHead(&pPagedPoolDesc->PoolHead);
	InitializeListHead(&pNonPagedPoolDesc->PoolHead);

	InsertTailList(
		&pPagedPoolDesc->PoolHead, 
		&pagedFirstBlock->PoolEntry
	);
	InsertTailList(
		&pNonPagedPoolDesc->PoolHead,
		&nonPagedFirstBlock->PoolEntry
	);

	return STATUS_SUCCESS;
}

static
VOID
DebugEnumeratePoolBlocks(
	POOL_TYPE poolType
)
{
	PPOOL_DESCRIPTOR poolDescriptor;
	PPOOL_HEADER current;
	SIZE_T totalSize;

	PrintT("----------------------------\n");
	poolDescriptor = Pools[poolType];

	if (poolDescriptor->PoolLock == 0)
		KeBugCheckEx(SPIN_LOCK_NOT_OWNED, __LINE__, 0, 0, 0);

	totalSize = 0;
	current = (PPOOL_HEADER)poolDescriptor->PoolHead.First;

	while (ExVerifyPoolBlock(current, poolType) && 
		(PLIST_ENTRY)current != &poolDescriptor->PoolHead)
	{
		PrintT(
			"%x: [%X] size=%i pooltype=%i, next: %X\n", 
			current, 
			current->AllocationTag,
			current->Size,
			(ULONG_PTR)current->PoolType,
			(ULONG_PTR)((ULONG_PTR)current + sizeof(POOL_HEADER) + current->Size)
		);

		if (current->Size > 0xFFFFFF)
		{
			PrintT("Error: invalid size 0x%X\n", current->Size);
			while (1);
		}

		totalSize += sizeof(POOL_HEADER) + current->Size;
		current = (PPOOL_HEADER)current->PoolEntry.Next;
	}

	if (ExVerifyPoolBlock(current, poolType))
	{
		PrintT(
			"Block %X invalid  :  %x %x %i %i\n", 
			current,
			current->PoolEntry.Next,
			current->PoolEntry.Prev,
			current->Size, 
			current->AllocationTag
		);
	}
}

NTSTATUS
ExPoolSelfCheck()
{
	PPOOL_DESCRIPTOR poolDescriptor;
	PVOID pageSizeAllocation;
	PVOID smallAllocation;
	KIRQL irql;

	PrintT("Testing pool\n");
	poolDescriptor = Pools[NonPagedPool];

	KeAcquireSpinLock(&poolDescriptor->PoolLock, &irql);
	DebugEnumeratePoolBlocks(NonPagedPool);
	KeReleaseSpinLock(&poolDescriptor->PoolLock, irql);

	smallAllocation = ExAllocatePoolWithTag(NonPagedPool, 16, 'TEST');
	pageSizeAllocation = ExAllocatePoolWithTag(NonPagedPool, 4096, 'TEST');

	KeAcquireSpinLock(&poolDescriptor->PoolLock, &irql);
	DebugEnumeratePoolBlocks(NonPagedPool);
	KeReleaseSpinLock(&poolDescriptor->PoolLock, irql);

	ExFreePool(smallAllocation);
	ExFreePool(pageSizeAllocation);

	KeAcquireSpinLock(&poolDescriptor->PoolLock, &irql);
	DebugEnumeratePoolBlocks(NonPagedPool);
	KeReleaseSpinLock(&poolDescriptor->PoolLock, irql);

	return STATUS_SUCCESS;
}