#pragma once

#include "List.h"

#include <llvm-c/Core.h>


struct LLVMBackend;
struct SkModule;
struct AstFunction;


LLVMTypeRef CanPassByValue(LLVMBackend* llb, SkModule* module, LLVMTypeRef type);

LLVMValueRef CreateFunction(LLVMBackend* llb, SkModule* module, const char* mangledName, LLVMTypeRef returnType, List<LLVMTypeRef>& paramTypes, bool varArgs, bool entryPoint, LLVMLinkage linkage, LLVMModuleRef dstModule);
void GenerateFunctionBody(LLVMBackend* llb, SkModule* module, AstFunction* function, LLVMValueRef llvmValue);
