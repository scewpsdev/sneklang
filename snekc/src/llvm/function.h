#pragma once

#include <llvm-c/Core.h>


LLVMValueRef CreateFunction(const char* mangledName, LLVMTypeRef returnType, int numParams, LLVMTypeRef* paramTypes, bool varArgs, LLVMModuleRef dstModule, LLVMLinkage linkage);
