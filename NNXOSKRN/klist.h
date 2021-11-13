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
	PVOID NNXAllocatorAlloc(UINT64 size);
	VOID NNXAllocatorFree(PVOID memory);
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

		if (first == NULL)
			return NULL;

		current = first->Next;

		while (current->Next)
		{
			current = current->Next;
		}
	
		if (current == NULL)
			return NULL;

		current->Next = (PKLINKED_LIST)ALLOC(sizeof(KLINKED_LIST));
		
		if (current->Next == NULL)
			return NULL;

		current->Next->Prev = current;
		current = current->Next;
		current->Next = NULL;
		current->Value = value;

		return current;
	}

	inline PKLINKED_LIST FindOnList(PKLINKED_LIST first, PVOID value)
	{
		PKLINKED_LIST current;

		if (first == NULL)
			return NULL;

		current = first->Next;

		while (current)
		{
			if (current->Value == value)
				return current;
			current = current->Next;
		}

		return NULL;
	}

	inline VOID RemoveFromList(PKLINKED_LIST first, PVOID value)
	{
		PKLINKED_LIST listElement = FindOnList(first, value);
		PKLINKED_LIST prev, next;
		
		if (listElement == NULL)
			return;
		
		prev = listElement->Prev;
		next = listElement->Next;

		if (first == NULL)
			return;

		if (prev != NULL)
		{
			prev->Next = NULL;
			prev = listElement->Prev = NULL;
		}

		if (next != NULL)
		{
			next->Prev = NULL;
			next = listElement->Next = NULL;
		}

		DEALLOC(listElement);
	}

	inline VOID ClearListAndDestroyValues(PKLINKED_LIST first, VOID(*Destroy)(PVOID))
	{
		PKLINKED_LIST current;

		if (first == NULL)
			return;

		current = first->Next;

		if (current == NULL)
			return;

		while (current)
		{
			PKLINKED_LIST next = current->Next;
			if(Destroy != NULL)
				Destroy(current->Value);
			DEALLOC(current);
			current = next;
		}
	}

	inline VOID ClearList(PKLINKED_LIST first)
	{
		ClearListAndDestroyValues(first, NULL);
	}

#ifdef __cplusplus
}
#endif

#endif