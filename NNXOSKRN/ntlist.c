#include "ntlist.h"

PLIST_ENTRY ExInterlockedInsertHeadList(
    PLIST_ENTRY ListHead,
    PLIST_ENTRY ListEntry,
    PKSPIN_LOCK Lock
)
{
    KIRQL irql;
    PLIST_ENTRY oldFirst;

    KeAcquireSpinLock(Lock, &irql);

    oldFirst = ListHead->First;
    InsertHeadList(ListHead, ListEntry);

    KeReleaseSpinLock(Lock, irql);

    if (oldFirst == ListHead)
        return NULL;

    return oldFirst;
}

PLIST_ENTRY ExInterlockedInsertTailList(
    PLIST_ENTRY ListHead,
    PLIST_ENTRY ListEntry,
    PKSPIN_LOCK Lock
)
{
    KIRQL irql;
    PLIST_ENTRY oldLast;

    KeAcquireSpinLock(Lock, &irql);

    oldLast = ListHead->Last;
    InsertTailList(ListHead, ListEntry);

    KeReleaseSpinLock(Lock, irql);

    if (oldLast == ListHead)
        return NULL;

    return oldLast;
}

PLIST_ENTRY ExInterlockedRemoveHeadList(
    PLIST_ENTRY ListHead,
    PKSPIN_LOCK Lock
)
{
    PLIST_ENTRY removedEntry;
    KIRQL irql;

    if (IsListEmpty(ListHead))
        return NULL;

    KeAcquireSpinLock(Lock, &irql);

    removedEntry = RemoveHeadList(ListHead);

    KeReleaseSpinLock(Lock, irql);

    return removedEntry;
}

PLIST_ENTRY ExInterlockedRemoveTailList(
    PLIST_ENTRY ListHead,
    PKSPIN_LOCK Lock
)
{
    PLIST_ENTRY removedEntry;
    KIRQL irql;

    if (IsListEmpty(ListHead))
        return NULL;

    KeAcquireSpinLock(Lock, &irql);

    removedEntry = RemoveTailList(ListHead);

    KeReleaseSpinLock(Lock, irql);

    return removedEntry;
}

PLIST_ENTRY_POINTER FindElementInPointerList(PLIST_ENTRY_POINTER Head, PVOID Pointer)
{
    PLIST_ENTRY_POINTER current = (PLIST_ENTRY_POINTER)Head->First;

    while (current)
    {
        if (current->Pointer == Pointer)
            return current;

        current = (PLIST_ENTRY_POINTER)current->Next;
    }

    return NULL;
}