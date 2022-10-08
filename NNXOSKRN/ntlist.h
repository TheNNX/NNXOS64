/* GNU EFI has definitions for LIST_ENTRY, apparently */
#ifdef _EFI_INCLUDE_
#define NNX_NT_LIST_HEADER
#endif

#ifndef NNX_NT_LIST_HEADER
#define NNX_NT_LIST_HEADER

#include <nnxtype.h>
#include <HAL/spinlock.h>

typedef struct _LIST_ENTRY
{
	union
	{
		struct _LIST_ENTRY* Flink;
		struct _LIST_ENTRY* Next;
		struct _LIST_ENTRY* First;
	};

	union
	{
		struct _LIST_ENTRY* Blink;
		struct _LIST_ENTRY* Prev;
		struct _LIST_ENTRY* Last;
	};
}LIST_ENTRY, * PLIST_ENTRY;

typedef struct _LIST_ENTRY_POINTER
{
    struct _LIST_ENTRY;
    PVOID Pointer;
} LIST_ENTRY_POINTER, * PLIST_ENTRY_POINTER;

typedef struct _SINGLE_LIST_ENTRY
{
    struct _SINGLE_LIST_ENTRY* Next;
}SINGLE_LIST_ENTRY, *PSINGLE_LIST_ENTRY;

#ifdef __cplusplus
extern "C" {
#endif
    inline VOID InitializeListHead(PLIST_ENTRY ListHead)
    {
        ListHead->Next = ListHead;
        ListHead->Prev = ListHead;
    }

    inline VOID InsertTailList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
    {
        PLIST_ENTRY OldLast = ListHead->Last;

        ListHead->Last = Entry;

        Entry->Next = ListHead;
        Entry->Prev = OldLast;

        OldLast->Next = Entry;
    }

    inline VOID InsertHeadList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
    {
        PLIST_ENTRY OldFirst = ListHead->First;

        ListHead->First = Entry;

        Entry->Next = OldFirst;
        Entry->Prev = ListHead;

        OldFirst->Prev = Entry;
    }

    inline PLIST_ENTRY RemoveHeadList(PLIST_ENTRY ListHead)
    {
        PLIST_ENTRY result = ListHead->First;
        ListHead->First = ListHead->First->Next;
        ListHead->First->Prev = ListHead;
        return result;
    }

    inline PLIST_ENTRY RemoveTailList(PLIST_ENTRY ListHead)
    {
        PLIST_ENTRY result = ListHead->Last;
        ListHead->Last = ListHead->Last->Prev;
        ListHead->Last->Next = ListHead;
        return result;
    }

    inline BOOLEAN RemoveEntryList(PLIST_ENTRY Entry)
    {
        PLIST_ENTRY Prev = Entry->Prev;
        PLIST_ENTRY Next = Entry->Next;

        Prev->Next = Next;
        Next->Prev = Prev;

        return (Prev == Next);
    }

    inline BOOLEAN IsListEmpty(PLIST_ENTRY Head)
    {
        return (Head->First == Head);
    }

    PLIST_ENTRY ExInterlockedInsertHeadList(
        PLIST_ENTRY ListHead,
        PLIST_ENTRY ListEntry,
        PKSPIN_LOCK Lock
    );

    PLIST_ENTRY ExInterlockedInsertTailList(
        PLIST_ENTRY ListHead,
        PLIST_ENTRY ListEntry,
        PKSPIN_LOCK Lock
    );

	PLIST_ENTRY ExInterlockedRemoveHeadList(
		PLIST_ENTRY ListHead,
		PKSPIN_LOCK Lock
	);

	PLIST_ENTRY ExInterlockedRemoveTailList(
		PLIST_ENTRY ListHead,
		PKSPIN_LOCK Lock
	);

    /**
     * @brief Finds a PLIST_ENTRY_POINTER with a given Pointer
     * @param Head - pointer to the list head
     * @param Pointer - desired pointer
     * @return NULL if the pointer was not found, entry with the pointer otherwise
    */
    PLIST_ENTRY_POINTER FindElementInPointerList(
        PLIST_ENTRY_POINTER Head,
        PVOID Pointer
    );

    inline void PushEntryList(
        PSINGLE_LIST_ENTRY Head,
        PSINGLE_LIST_ENTRY Entry
    )
    {
        PSINGLE_LIST_ENTRY oldHeadNext;

        oldHeadNext = Head->Next;
        Head->Next = Entry;
        Entry->Next = oldHeadNext;
    }

    inline PSINGLE_LIST_ENTRY PopEntryList(
        PSINGLE_LIST_ENTRY Head
    )
    {
        PSINGLE_LIST_ENTRY result;
        result = Head->Next;
        if (result != NULL)
            Head->Next = result->Next;
        return result;
    }

#ifdef __cplusplus
}
#endif

#define CONTAINING_RECORD(address, type, field) \
((type*)((ULONG_PTR)address - (ULONG_PTR)(&((type*)NULL)->field)))

#endif