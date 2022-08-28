#pragma once

#include "LLVMBackend.h"


LLVMValueRef GenGenericFunctionInstance(LLVMBackend* llb, SkModule* module, AST::Function* function, List<LLVMTypeRef>& genericArgs, TypeID& functionType);
LLVMTypeRef GenGenericStructInstance(LLVMBackend* llb, SkModule* module, AST::Struct* str, List<LLVMTypeRef>& genericArgs);
