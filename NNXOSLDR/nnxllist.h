#pragma once
#include "memory/nnxalloc.h"
#ifdef __cplusplus
template<typename T> struct NNXLinkedListEntry {
public:
	T value;
	NNXLinkedListEntry* next;
};

template<typename T> class NNXLinkedList {
public:
	NNXLinkedListEntry<T>* optimized;
	NNXLinkedListEntry<T>* first;

	NNXLinkedList() {
		this->first = 0;
		this->optimized = this->first;
	}

	NNXLinkedList(T element) {
		this->first = (NNXLinkedListEntry<T>*)nnxmalloc(sizeof(NNXLinkedListEntry<T>));
		*this->first = NNXLinkedListEntry<T>();
		this->first->next = 0;
		this->first->value = element;
		this->optimized = this->first;
	}

	bool contains(T element) {

		NNXLinkedListEntry<T>* cur = this->first;

		while (cur) {
			if (cur->value == element) {
				return true;
			}

			cur = cur->next;
		}

		return false;
	}

	NNXLinkedListEntry<T>* add(T element) {

		if (this->optimized == 0)
			this->optimized = this->first;

		NNXLinkedListEntry<T>** cur = &(this->optimized);

		while (*cur) {
			cur = &((*cur)->next);
		}

		*cur = (NNXLinkedListEntry<T>*)nnxmalloc(sizeof(NNXLinkedListEntry<T>));
		**cur = NNXLinkedListEntry<T>();
		(*cur)->next = 0;
		(*cur)->value = element;
		this->optimized = *cur;
		if (this->first == 0) {
			this->first = this->optimized;
		}
		return *cur;
	}

	T remove(T element) {
		NNXLinkedListEntry<T>* cur = this->first;

		NNXLinkedListEntry<T>* last = 0;

		while (cur) {
			if (cur->value == element) {

				if (last) {
					last->next = cur->next;
				}
				else {
					this->first = cur->next;
				}

				this->optimized = this->first;
				T value = cur->value;
				nnxfree(cur);

				return value;
			}

			last = cur;
			cur = cur->next;
		}

		this->optimized = this->first;

		T zero;
		*((int*)(&zero)) = 0;
		return zero;
	}

	T remove(NNXLinkedListEntry<T>* element) {
		NNXLinkedListEntry<T>* cur = this->first;
		NNXLinkedListEntry<T>* last = 0;

		while (cur) {
			if (cur == element) {

				if (last) {
					last->next = cur->next;
				}
				else {
					this->first = cur->next;
				}

				this->optimized = this->first;
				T value = cur->value;
				nnxfree(cur);

				return value;
			}

			last = cur;
			cur = cur->next;
		}

		T zero;
		*((int*)(&zero)) = 0;
		this->optimized = this->first;
		return zero;
	}

	T* toArray(int* size) {
		NNXLinkedListEntry<T>* cur = this->first;
		int numberOfElements = 0;

		while (cur) {
			numberOfElements++;
			cur = cur->next;
		}

		T* array = nnxcalloc(sizeof(T), numberOfElements);

		int indexOfElements = 0;
		cur = this->first;
		while (cur) {
			array[indexOfElements] = cur->value;
			indexOfElements++;
			cur = cur->next;
		}

		*size = numberOfElements;
		return array;
	}
};
template<typename K, typename V> struct NNXDictionaryListEntry {
public:
	V value;
	K key;
	NNXDictionaryListEntry<K, V>* next;

};

template<typename K, typename V> class NNXDictionary {
public:
	NNXDictionaryListEntry<K, V>* first;

	NNXDictionary() {
		this->first = 0;
	}

	NNXDictionary(K key, V element) {
		this->first = (NNXDictionaryListEntry<K, V>*)nnxmalloc(sizeof(NNXDictionaryListEntry<K, V>))
		*this->first = NNXDictionaryListEntry<K, V>();
		this->first->next = 0;
		this->first->value = element;
		this->first->key = key;
	}

	bool containsElement(V element) {
		NNXDictionaryListEntry<K, V>* cur = this->first;

		while (cur) {
			if (cur->value == element) {
				return true;
			}

			cur = cur->next;
		}

		return false;
	}

	bool containsKey(K key) {
		NNXDictionaryListEntry<K, V>* cur = this->first;

		while (cur) {
			if (cur->key == key) {
				return true;
			}

			cur = cur->next;
		}

		return false;
	}

	NNXDictionaryListEntry<K, V>* add(K key, V element) {
		NNXDictionaryListEntry<K, V>** cur = &(this->first);

		while (*cur) {
			cur = &((*cur)->next);
		}

		*cur = (NNXDictionaryListEntry<K, V>*)nnxmalloc(sizeof(NNXDictionaryListEntry<K, V>));
		**cur = NNXDictionaryListEntry<K, V>();
		(*cur)->next = 0;
		(*cur)->value = element;
		(*cur)->key = key;

		return *cur;
	}

	V remove(V element) {
		NNXDictionaryListEntry<K, V>* cur = this->first;
		NNXDictionaryListEntry<K, V>* last = 0;

		while (cur) {
			if (cur->value == element) {

				if (last) {
					last->next = cur->next;
				}
				else {
					this->first = cur->next;
				}

				V value = cur->value;
				nnxfree(cur);

				return value;
			}

			last = cur;
			cur = cur->next;
		}

		V zero;
		*((int*)(&zero)) = 0;
		return zero;
	}

	V getValue(K key) {
		NNXDictionaryListEntry<K, V>* cur = this->first;

		while (cur) {
			if (cur->key == key) {
				return cur->value;
			}

			cur = cur->next;
		}

		V zero;
		*((int*)(&zero)) = 0;
		return zero;
	}


	V* toArray(int* size) {
		NNXDictionaryListEntry<K, V>* cur = this->first;
		int numberOfElements = 0;

		while (cur) {
			numberOfElements++;
			cur = cur->next;
		}

		V* array = nnxcalloc(sizeof(V), numberOfElements);

		int indexOfElements = 0;
		cur = this->first;
		while (cur) {
			array[indexOfElements] = cur->value;
			indexOfElements++;
			cur = cur->next;
		}

		*size = numberOfElements;
		return array;
	}
};
#endif