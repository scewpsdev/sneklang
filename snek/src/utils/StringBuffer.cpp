#include "stringbuffer.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))


StringBuffer CreateStringBuffer(int capacity) {
	StringBuffer buffer;

	buffer.buffer = new char[capacity + 1];
	buffer.length = 0;
	buffer.capacity = capacity;

	buffer.buffer[0] = 0;

	return buffer;
}

void DestroyStringBuffer(StringBuffer& buffer) {
	delete[] buffer.buffer;
}

void StringBufferResize(StringBuffer& buffer, int newCapacity) {
	char* newBuffer = new char[newCapacity + 1];
	int lengthToCopy = min(newCapacity, buffer.capacity);
	memcpy(newBuffer, buffer.buffer, lengthToCopy * sizeof(char));
	newBuffer[newCapacity] = 0;
	delete[] buffer.buffer;

	buffer.buffer = newBuffer;
	buffer.capacity = newCapacity;
	buffer.length = min(buffer.length, newCapacity);
}

void StringBufferAppend(StringBuffer& buffer, const char* str) {
	int len = (int)strlen(str);
	if (buffer.length + len > buffer.capacity) {
		StringBufferResize(buffer, buffer.capacity + len + max(buffer.capacity / 2, 1));
	}
	memcpy(buffer.buffer + buffer.length, str, len * sizeof(char));
	buffer.length += len;
	buffer.buffer[buffer.length] = 0;
}

void StringBufferAppend(StringBuffer& buffer, char c) {
	if (buffer.length + 1 > buffer.capacity) {
		StringBufferResize(buffer, buffer.capacity + max(buffer.capacity / 2, 1));
	}
	buffer.buffer[buffer.length++] = c;
	buffer.buffer[buffer.length] = 0;
}

void StringBufferAppend(StringBuffer& buffer, int i) {
	char str[16];
	_itoa(i, str, 10);
	StringBufferAppend(buffer, str);
}

void StringBufferAppend(StringBuffer& buffer, unsigned long ul)
{
	char str[32];
	_ultoa(ul, str, 10);
	StringBufferAppend(buffer, str);
}

void StringBufferAppend(StringBuffer& buffer, double d, int fpDigits)
{
	char str[32];
	sprintf(str, "%.*f", fpDigits, d);
	StringBufferAppend(buffer, str);
}

void StringBufferAppend(StringBuffer& buffer, double d)
{
	char str[32];
	sprintf(str, "%.14e", d);
	StringBufferAppend(buffer, str);
}

void StringBufferAppend(StringBuffer& buffer, bool b)
{
	if (b)
		StringBufferAppend(buffer, "true");
	else
		StringBufferAppend(buffer, "false");
}
