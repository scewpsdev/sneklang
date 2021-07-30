#pragma once

#include <stdio.h>


#define SnekAssert(condition, message) { if (!(condition)) { printf("Assertion %s failed at %s line %d: %s", #condition, __FILE__, __LINE__, message); __debugbreak(); } }


enum MessageType
{
	MESSAGE_TYPE_NULL = 0,

	MESSAGE_TYPE_INFO,
	MESSAGE_TYPE_WARNING,
	MESSAGE_TYPE_ERROR,
	MESSAGE_TYPE_FATAL_ERROR,
};

typedef void(*MessageCallback_t)(MessageType msgType, const char* msg, ...);
