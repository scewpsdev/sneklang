#pragma once

#include "utils/List.h"
#include "semantics/Type.h"

#include <llvm-c/Core.h>


struct LLVMBackend;
struct SkModule;

namespace AST
{
	struct Function;
}


LLVMTypeRef CanPassByValue(LLVMBackend* llb, SkModule* module, LLVMTypeRef type);

LLVMTypeRef FunctionType(LLVMBackend* llb, SkModule* module, LLVMTypeRef returnType, LLVMTypeRef* paramTypes, int numParams, bool varArgs);

LLVMValueRef CreateFunction(LLVMBackend* llb, SkModule* module, const char* mangledName, LLVMTypeRef returnType, List<LLVMTypeRef>& paramTypes, bool varArgs, bool entryPoint, LLVMLinkage linkage, LLVMModuleRef dstModule);
void GenerateFunctionBody(LLVMBackend* llb, SkModule* module, AST::Function* function, LLVMValueRef llvmValue);
LLVMValueRef CallFunction(LLVMBackend* llb, SkModule* module, TypeID functionType, LLVMValueRef callee, LLVMValueRef* args, int numArgs);
