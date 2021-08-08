#pragma once

#include <string.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))


template<typename T>
struct List
{
	T* buffer = NULL;
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

	template<typename T>
	void reserve(int newCapacity)
	{
		T* newBuffer = new T[newCapacity];
		if (this->buffer)
		{
			int numCopiedElements = min(newCapacity, this->size);
			memcpy(newBuffer, this->buffer, numCopiedElements * sizeof(T));
			delete[] this->buffer;
		}
		this->buffer = newBuffer;
		this->capacity = newCapacity;
		this->size = min(this->size, newCapacity);
	}

	template<typename T>
	void resize(int newSize)
	{
		if (newSize > this->capacity)
		{
			reserve(newSize);
		}
		this->size = newSize;
	}

	template<typename T>
	void add(const T& t)
	{
		while (this->size + 1 > this->capacity)
		{
			reserve(this->capacity + max(this->capacity / 2, 1));
		}
		this->buffer[this->size++] = t;
	}

	template<typename T>
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

	template<typename T>
	int indexOf(const T& t)
	{
		for (int i = 0; i < this->size; i++)
		{
			if (this->buffer[i] == t)
				return i;
		}
		return -1;
	}

	template<typename T>
	void remove(const T& t)
	{
		int idx = indexOf(t);
		if (idx != -1)
			removeAt(idx);
	}

	template<typename T>
	void clear()
	{
		this->size = 0;
	}
};


template<typename T>
List<T> CreateList(int capacity = 0)
{
	List<T> list = {};

	if (capacity > 0)
	{
		list.buffer = new T[capacity];
		list.capacity = capacity;
		list.size = 0;
	}

	return list;
}

template<typename T>
void DestroyList(List<T>& list)
{
	delete list.buffer;
}

template<typename T>
void ListReserve(List<T>& list, int newCapacity)
{
	T* newBuffer = new T[newCapacity];
	if (list.buffer)
	{
		int numCopiedElements = min(newCapacity, list.size);
		memcpy(newBuffer, list.buffer, numCopiedElements * sizeof(T));
		delete[] list.buffer;
	}
	list.buffer = newBuffer;
	list.capacity = newCapacity;
	list.size = min(list.size, newCapacity);
}

template<typename T>
void ListResize(List<T>& list, int newSize)
{
	if (newSize > list.capacity)
	{
		ListReserve(list, newSize);
	}
	list.size = newSize;
}

template<typename T>
void ListAdd(List<T>& list, const T& t)
{
	while (list.size + 1 > list.capacity)
	{
		ListReserve(list, list.capacity + max(list.capacity / 2, 1));
	}
	list.buffer[list.size++] = t;
}

template<typename T>
void ListRemoveAt(List<T>& list, int idx)
{
	if (idx < list.size)
	{
		for (int i = idx; i < list.size - 1; i++)
		{
			list.buffer[i] = list.buffer[i + 1];
		}
		list.size--;
	}
	else
	{
		__debugbreak();
	}
}
