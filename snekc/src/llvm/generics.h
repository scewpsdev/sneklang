#pragma once

#include "llvm_backend.h"


LLVMValueRef GenGenericFunctionInstance(LLVMBackend* llb, SkModule* module, AstFunction* function, List<LLVMTypeRef>& genericArgs, TypeID& functionType);
