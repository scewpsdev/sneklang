#pragma once

#include <inttypes.h>

#include <memory>


template<typename T>
T min(T a, T b)
{
	return a < b ? a : b;
}

template<typename T>
T max(T a, T b)
{
	return a > b ? a : b;
}

int64_t getNanos();

using defer = std::shared_ptr<void>;
