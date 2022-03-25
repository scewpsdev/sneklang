#pragma once

#include <stdint.h>


template<typename T>
bool HasFlag(T flags, T flag)
{
	return ((uint32_t)flags & (uint32_t)flag) == (uint32_t)flag;
}
