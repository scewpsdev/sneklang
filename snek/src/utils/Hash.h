#pragma once

#include <stdint.h>


uint32_t hash(uint32_t i);
uint32_t hash(float f);

uint32_t hash(const char* str);

uint32_t hash(const void* ptr);

uint32_t hashCombine(uint32_t h0, uint32_t h1);
