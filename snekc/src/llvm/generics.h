#pragma once

#include "llvm_backend.h"


LLVMValueRef GenGenericFunctionInstance(LLVMBackend* llb, SkModule* module, AST::Function* function, List<LLVMTypeRef>& genericArgs, TypeID& functionType);
