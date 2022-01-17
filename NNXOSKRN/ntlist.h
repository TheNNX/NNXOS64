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

    inline BOOL RemoveEntryList(PLIST_ENTRY Entry)
    {
        PLIST_ENTRY Prev = Entry->Prev;
        PLIST_ENTRY Next = Entry->Next;

        Prev->Next = Next;
        Next->Prev = Prev;

        return (Prev == Next);
    }

    inline BOOL IsListEmpty(PLIST_ENTRY Head)
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


#ifdef __cplusplus
}
#endif

#endif