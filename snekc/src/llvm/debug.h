#pragma once

//#define LOG_LLVM_CALLS

#if defined(LOG_LLVM_CALLS)

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define __FILENAME__ (strrchr("\\" __FILE__, '\\') + 1)
#else
#define __FILENAME__ (strrchr("/" __FILE__, '/') + 1)
#endif

#define LLVM_CALL(x, ...) [=](const char* funcName, const char* file, int line) {\
	fprintf(stderr, "(%s, line %d): %s(%s)\n", file, line, funcName, #__VA_ARGS__);\
	return x(##__VA_ARGS__);\
}(#x, __FILENAME__, __LINE__)

#else

#define LLVM_CALL(x, ...) x(__VA_ARGS__)

#endif
