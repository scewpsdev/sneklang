#include "function.h"

#include "debug.h"


LLVMValueRef CreateFunction(const char* mangledName, LLVMTypeRef returnType, int numParams, LLVMTypeRef* paramTypes, bool varArgs, LLVMModuleRef dstModule, LLVMLinkage linkage)
{
	LLVMTypeRef functionType = LLVM_CALL(LLVMFunctionType, returnType, paramTypes, numParams, varArgs);
	LLVMValueRef function = LLVM_CALL(LLVMAddFunction, dstModule, mangledName, functionType);

	LLVM_CALL(LLVMSetLinkage, function, linkage);
	return function;
}
