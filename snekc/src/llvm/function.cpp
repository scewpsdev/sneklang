#include "function.h"

#include "ast.h"
#include "llvm_backend.h"
#include "debug.h"
#include "List.h"
#include "debug_info.h"
#include "values.h"


LLVMTypeRef CanPassByValue(LLVMBackend* llb, SkModule* module, LLVMTypeRef type)
{
	LLVMTypeKind typeKind = LLVMGetTypeKind(type);
	if (typeKind == LLVMStructTypeKind)
	{
		unsigned long long size = LLVMSizeOfTypeInBits(llb->targetData, type);
		if (size == 1 || size == 2 || size == 4 || size == 8 || size == 16 || size == 32 || size == 64)
			return LLVMIntTypeInContext(llb->llvmContext, (unsigned int)size);
		else
			return NULL;
	}
	else
	{
		return type;
	}
}

LLVMValueRef CreateFunction(LLVMBackend* llb, SkModule* module, const char* mangledName, LLVMTypeRef returnType, List<LLVMTypeRef>& paramTypes, bool varArgs, bool entryPoint, LLVMLinkage linkage, LLVMModuleRef dstModule)
{
	LLVMTypeRef functionReturnType = NULL;
	List<LLVMTypeRef> functionParamTypes = CreateList<LLVMTypeRef>();

	if (entryPoint)
	{
		functionReturnType = LLVMInt32TypeInContext(llb->llvmContext);
	}
	else
	{
		if (LLVMTypeRef _returnType = CanPassByValue(llb, module, returnType))
		{
			functionReturnType = _returnType;
		}
		else
		{
			functionReturnType = LLVMVoidTypeInContext(llb->llvmContext);
			functionParamTypes.add(LLVMPointerType(returnType, 0));
		}
	}

	for (int i = 0; i < paramTypes.size; i++)
	{
		LLVMTypeRef paramType = paramTypes[i];
		if (LLVMTypeRef _paramType = CanPassByValue(llb, module, paramType))
		{
			functionParamTypes.add(_paramType);
		}
		else
		{
			functionParamTypes.add(LLVMPointerType(paramType, 0));
		}
	}

	LLVMTypeRef functionType = LLVM_CALL(LLVMFunctionType, returnType, paramTypes.buffer, paramTypes.size, varArgs);
	LLVMValueRef function = LLVM_CALL(LLVMAddFunction, dstModule, mangledName, functionType);

	LLVM_CALL(LLVMSetLinkage, function, linkage);
	return function;
}

void GenerateFunctionBody(LLVMBackend* llb, SkModule* module, AstFunction* function, LLVMValueRef llvmValue)
{
	LLVMTypeRef returnType = GenType(llb, module, function->returnType);
	bool returnValueAsArg = !CanPassByValue(llb, module, returnType);

	LLVMBasicBlockRef parentBlock = LLVM_CALL(LLVMGetInsertBlock, module->builder);
	LLVMBasicBlockRef entryBlock = LLVM_CALL(LLVMAppendBasicBlockInContext, llb->llvmContext, llvmValue, "entry");
	LLVMBasicBlockRef returnBlock = LLVM_CALL(LLVMCreateBasicBlockInContext, llb->llvmContext, "return");
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, entryBlock);

	if (module->hasDebugInfo)
	{
		LLVMMetadataRef subProgram = DebugInfoBeginFunction(llb, module, function, llvmValue, NULL);
		DebugInfoEmitNullLocation(llb, module, module->builder);
		DebugInfoEmitNullLocation(llb, module, module->entryBuilder);
	}

	module->returnBlock = returnBlock;
	module->returnAlloc = NULL;
	if (function->returnType->typeKind != TYPE_KIND_VOID)
	{
		if (!returnValueAsArg)
		{
			module->returnAlloc = AllocateLocalVariable(llb, module, returnType, "ret_val");
			LLVMBuildStore(module->builder, LLVMConstNull(returnType), module->returnAlloc);
		}
		else
		{
			module->returnAlloc = LLVMGetParam(llvmValue, 0);
		}
	}

	for (int i = 0; i < function->paramTypes.size; i++) {
		LLVMValueRef arg = NULL;
		if (!returnValueAsArg)
			arg = LLVM_CALL(LLVMGetParam, llvmValue, i);
		else
			arg = LLVM_CALL(LLVMGetParam, llvmValue, i + 1);

		LLVMTypeRef paramType = GenType(llb, module, function->paramTypes[i]);
		LLVMValueRef argAlloc = NULL;

		if (LLVMTypeRef _paramType = CanPassByValue(llb, module, paramType))
		{
			argAlloc = AllocateLocalVariable(llb, module, paramType, function->paramNames[i]);
			if (paramType != _paramType)
			{
				LLVMValueRef bitcasted = LLVMBuildBitCast(module->builder, argAlloc, LLVMPointerType(_paramType, 0), "");
				LLVM_CALL(LLVMBuildStore, module->builder, arg, bitcasted);
			}
			else
			{
				LLVM_CALL(LLVMBuildStore, module->builder, arg, argAlloc);
			}
		}
		else
		{
			argAlloc = arg;
		}

		if (module->hasDebugInfo)
		{
			DebugInfoDeclareParameter(llb, module, argAlloc, i, function->paramTypes[i]->typeID, function->paramNames[i], function->paramTypes[i]->inputState.line, function->paramTypes[i]->inputState.col);
		}

		function->paramVariables[i]->allocHandle = argAlloc;
	}

	GenStatement(llb, module, function->body);

	LLVM_CALL(LLVMAppendExistingBasicBlock, llvmValue, returnBlock);

	if (!BlockHasBranched(llb, module))
		LLVM_CALL(LLVMBuildBr, module->builder, returnBlock);
	if (LLVM_CALL(LLVMGetInsertBlock, module->builder) != returnBlock)
		LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, returnBlock);

	// Emit return location
	if (module->hasDebugInfo)
	{
		DebugInfoEmitSourceLocation(llb, module, module->builder, function->endInputState.line, function->endInputState.col);
	}

	if (function->returnType->typeKind != TYPE_KIND_VOID)
	{
		if (!returnValueAsArg)
			LLVM_CALL(LLVMBuildRet, module->builder, LLVM_CALL(LLVMBuildLoad, module->builder, module->returnAlloc, ""));
		else
			LLVM_CALL(LLVMBuildRetVoid, module->builder);
	}
	else
	{
		if (function->isEntryPoint)
		{
			LLVM_CALL(LLVMBuildRet, module->builder, LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 0, false));
		}
		else
		{
			LLVM_CALL(LLVMBuildRetVoid, module->builder);
		}
	}

	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, parentBlock);

	if (module->hasDebugInfo)
	{
		DebugInfoEndFunction(llb, module, function);
	}
}
