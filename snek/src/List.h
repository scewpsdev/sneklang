#pragma once

#include <stdlib.h>
#include <string.h>


inline int __min2(int a, int b) { return a < b ? a : b; }
inline int __max2(int a, int b) { return a > b ? a : b; }


template<typename T, typename U>
struct Pair
{
	T first;
	U second;
};

template<typename T, typename U>
Pair<T, U> CreatePair(const T& t, const U& u)
{
	Pair<T, U> pair;

	pair.first = t;
	pair.second = u;

	return pair;
}

template<typename T>
struct List
{
	T* buffer = nullptr;
	int capacity = 0;
	int size = 0;


	T& operator[](int idx)
	{
		if (idx < this->size)
		{
			return this->buffer[idx];
		}
		else
		{
			__debugbreak();
			return this->buffer[0];
		}
	}

	const T& operator[](int idx) const
	{
		if (idx < this->size)
		{
			return this->buffer[idx];
		}
		else
		{
			__debugbreak();
			return this->buffer[0];
		}
	}

	T& front()
	{
		return (*this)[0];
	}

	T& back()
	{
		return (*this)[size - 1];
	}

	void reserve(int newCapacity)
	{
		T* newBuffer = (T*)malloc(sizeof(T) * newCapacity);
		if (this->buffer)
		{
			int numCopiedElements = __min2(newCapacity, this->size);
			memcpy(newBuffer, this->buffer, numCopiedElements * sizeof(T));
			delete this->buffer;
		}
		this->buffer = newBuffer;
		this->capacity = newCapacity;
		this->size = __min(this->size, newCapacity);
	}

	void resize(int newSize)
	{
		if (newSize > this->capacity)
		{
			reserve(newSize);
		}
		this->size = newSize;
	}

	void add(const T& t)
	{
		while (this->size + 1 > this->capacity)
		{
			reserve(this->capacity + __max2(this->capacity / 2, 1));
		}
		this->buffer[this->size++] = t;
	}

	void addAll(const List<T>& list)
	{
		while (this->size + list.size > this->capacity)
		{
			reserve(this->capacity + __max2(this->capacity / 2, 1));
		}
		for (int i = 0; i < list.size; i++)
		{
			add(list[i]);
		}
	}

	void removeAt(int idx)
	{
		if (idx < this->size)
		{
			for (int i = idx; i < this->size - 1; i++)
			{
				this->buffer[i] = this->buffer[i + 1];
			}
			this->size--;
		}
		else
		{
			__debugbreak();
		}
	}

	int indexOf(const T& t)
	{
		for (int i = 0; i < this->size; i++)
		{
			if (this->buffer[i] == t)
				return i;
		}
		return -1;
	}

	bool contains(const T& t)
	{
		return indexOf(t) != -1;
	}

	void remove(const T& t)
	{
		int idx = indexOf(t);
		if (idx != -1)
			removeAt(idx);
	}

	void clear()
	{
		this->size = 0;
	}

	typedef T* iterator;
	typedef const T* const_iterator;

	iterator begin()
	{
		return size > 0 ? &buffer[0] : nullptr;
	}

	iterator end()
	{
		return size > 0 ? &buffer[size] : nullptr;
	}

	const_iterator begin() const
	{
		return size > 0 ? &buffer[0] : nullptr;
	}

	const_iterator end() const
	{
		return size > 0 ? &buffer[size] : nullptr;
	}

	List<T> clone() const
	{
		List<T> newList;
		newList.reserve(size);
		newList.addAll(*this);
		return newList;
	}
};


template<typename T>
List<T> CreateList(int capacity = 0)
{
	List<T> list;

	if (capacity > 0)
	{
		list.buffer = (T*)malloc(sizeof(T) * capacity);
		list.capacity = capacity;
		list.size = 0;
	}
	else
	{
		list.buffer = nullptr;
	}

	return list;
}

template<typename T>
void DestroyList(List<T>& list)
{
	if (list.buffer)
		delete list.buffer;
}

template<typename T>
void Sort(List<T>& list, int(*comparator)(T element0, T element1))
{
	for (int i = 0; i < list.size - 1; i++)
	{
		for (int j = 0; j < list.size - 1; j++)
		{
			T element0 = list[j];
			T element1 = list[j + 1];
			int comparison = comparator(element0, element1);
			if (comparison == -1)
			{
				// Swap
				list[j] = element1;
				list[j + 1] = element0;
			}
		}
	}
}

template<typename T>
void Sort(List<T>& list)
{
	Sort(list, [](const T& element0, const T& element1) -> int
		{
			if (element1 > element0) return 1;
			else if (element1 < element0) return -1;
			else return 0;
		});
}
