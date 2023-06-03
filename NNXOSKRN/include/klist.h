/*
	TODO: untested
*/

#ifndef NNX_KLIST_HEADER
#define NNX_KLIST_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ALLOC
#include <nnxalloc.h>
#define ALLOC(x) NNXAllocatorAlloc(x)
#define DEALLOC(x) NNXAllocatorFree(x)
#endif

	typedef struct _KLINKED_LIST KLINKED_LIST, *PKLINKED_LIST;
	struct _KLINKED_LIST
	{
		PKLINKED_LIST Prev;
		PKLINKED_LIST Next;
		PVOID Value;
	};

	inline PKLINKED_LIST AppendList(PKLINKED_LIST first, PVOID value)
	{
		PKLINKED_LIST current;

		if ((PVOID)first == NULL)
			return (PKLINKED_LIST)NULL;

		current = first;

		while (current->Next)
		{
			current = current->Next;
		}
	
		if ((PVOID)current == NULL)
			return (PKLINKED_LIST) NULL;

		current->Next = (PKLINKED_LIST)ALLOC(sizeof(KLINKED_LIST));
		
		if ((PVOID)current->Next == NULL)
			return (PKLINKED_LIST) NULL;

		current->Next->Prev = current;
		current = current->Next;
		current->Next = (PKLINKED_LIST) NULL;
		current->Value = value;

		return current;
	}

	inline PKLINKED_LIST FindInListCustomCompare(PKLINKED_LIST first, PVOID value, BOOL(*compare)(PVOID, PVOID))
	{
		PKLINKED_LIST current;

		if ((PVOID)first == NULL)
			return (PKLINKED_LIST) NULL;

		current = first->Next;

		while (current)
		{
			if (compare(current->Value, value))
				return current;
			current = current->Next;
		}

		return (PKLINKED_LIST) NULL;
	}

	inline BOOL __CompareEq(PVOID a, PVOID b)
	{
		return a == b;
	}

	inline PKLINKED_LIST FindInList(PKLINKED_LIST first, PVOID value)
	{
		return FindInListCustomCompare(first, value, __CompareEq);
	}

	inline VOID RemoveChainElementFromList(PKLINKED_LIST element)
	{
		if (element->Next)
			element->Next->Prev = element->Prev;
		if (element->Prev)
			element->Prev->Next = element->Next;

		DEALLOC(element);
	}

	inline VOID RemoveFromList(PKLINKED_LIST first, PVOID value)
	{
		PKLINKED_LIST listElement = FindInList(first, value);
		PKLINKED_LIST prev, next;
		
		if ((PVOID)listElement == NULL)
			return;
		
		prev = listElement->Prev;
		next = listElement->Next;

		if ((PVOID)first == NULL)
			return;

		if ((PVOID)prev != NULL)
		{
			prev->Next = (PKLINKED_LIST) NULL;
			prev = listElement->Prev = (PKLINKED_LIST) NULL;
		}

		if ((PVOID)next != NULL)
		{
			next->Prev = (PKLINKED_LIST) NULL;
			next = listElement->Next = (PKLINKED_LIST) NULL;
		}

		DEALLOC(listElement);
	}

	inline VOID ClearListAndDestroyValues(PKLINKED_LIST first, VOID(*Destroy)(PVOID))
	{
		PKLINKED_LIST current;

		if (first == (PKLINKED_LIST) NULL)
			return;

		current = first->Next;

		if (current == (PKLINKED_LIST) NULL)
			return;

		while (current)
		{
			PKLINKED_LIST next = current->Next;
			if((PVOID)Destroy != NULL)
				Destroy(current->Value);
			DEALLOC(current);
			current = next;
		}
	}

	inline VOID ClearList(PKLINKED_LIST first)
	{
		ClearListAndDestroyValues(first, (VOID(*)(PVOID))NULL);
	}

#ifdef __cplusplus
}
#endif

#endif