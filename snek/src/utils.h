#pragma once

#include <inttypes.h>

#include <memory>


int min(int a, int b);
int max(int a, int b);

int64_t getNanos();

using defer = std::shared_ptr<void>;
