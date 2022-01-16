#pragma once

#include "utils.h"

#include <string.h>


template<typename T>
struct List
{
	T* buffer = NULL;
	int capacity = 0;
	int size = 0;


	~List()
	{
		if (buffer)
		{
			free(buffer);
			buffer = NULL;
		}
	}

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

	T& last()
	{
		if (this->size > 0)
		{
			return this->buffer[this->size - 1];
		}
		else
		{
			__debugbreak();
			return this->buffer[0];
		}
	}

	void reserve(int newCapacity)
	{
		T* newBuffer = new T[newCapacity];
		if (this->buffer)
		{
			int numCopiedElements = min(newCapacity, this->size);
			memcpy(newBuffer, this->buffer, numCopiedElements * sizeof(T));
			delete this->buffer;
		}
		this->buffer = newBuffer;
		this->capacity = newCapacity;
		this->size = min(this->size, newCapacity);
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
			reserve(this->capacity + max(this->capacity / 2, 1));
		}
		this->buffer[this->size++] = t;
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
