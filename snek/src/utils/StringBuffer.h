#pragma once


struct StringBuffer {
	char* buffer;
	int length;
	int capacity;
};


StringBuffer CreateStringBuffer(int capacity);
void DestroyStringBuffer(StringBuffer& buffer);

void StringBufferResize(StringBuffer& buffer, int newCapacity);
void StringBufferAppend(StringBuffer& buffer, const char* str);
void StringBufferAppend(StringBuffer& buffer, char c);
void StringBufferAppend(StringBuffer& buffer, int i);
void StringBufferAppend(StringBuffer& buffer, unsigned long ull);
void StringBufferAppend(StringBuffer& buffer, double d, int fpDigits);
void StringBufferAppend(StringBuffer& buffer, double d);
void StringBufferAppend(StringBuffer& buffer, bool b);


template<typename T>
StringBuffer& operator<<(StringBuffer& buffer, T t)
{
	StringBufferAppend(buffer, t);
	return buffer;
}
