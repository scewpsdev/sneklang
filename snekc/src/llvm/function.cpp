#include "function.h"

#include "LLVMBackend.h"
#include "Debug.h"
#include "DebugInfo.h"
#include "Values.h"
#include "utils/List.h"
#include "utils/Log.h"

#include "ast/File.h"
#include "semantics/Variable.h"


LLVMTypeRef CanPassByValue(LLVMBackend* llb, SkModule* module, LLVMTypeRef type)
{
	LLVMTypeKind typeKind = LLVMGetTypeKind(type);
	if (typeKind == LLVMStructTypeKind || typeKind == LLVMArrayTypeKind)
	{
		unsigned long long size = LLVMSizeOfTypeInBits(llb->targetData, type);
		if (size == 1 || size == 2 || size == 4 || size == 8 || size == 16 || size == 32 || size == 64)
			return LLVMIntTypeInContext(llb->llvmContext, (unsigned int)size);
		else
			return nullptr;
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

	LLVMTypeRef functionType = LLVM_CALL(LLVMFunctionType, functionReturnType, functionParamTypes.buffer, functionParamTypes.size, varArgs);
	LLVMValueRef function = LLVM_CALL(LLVMAddFunction, dstModule, mangledName, functionType);
	LLVM_CALL(LLVMSetLinkage, function, linkage);

	DestroyList(functionParamTypes);

	return function;
}

void GenerateFunctionBody(LLVMBackend* llb, SkModule* module, AST::Function* function, LLVMValueRef llvmValue)
{
	LLVMTypeRef returnType = GenTypeID(llb, module, function->functionType->functionType.returnType);
	LLVMTypeRef functionReturnType = CanPassByValue(llb, module, returnType);
	bool returnValueAsArg = !functionReturnType;

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

	LLVMBasicBlockRef parentReturnBlock = module->returnBlock;
	LLVMValueRef parentReturnAlloc = module->returnAlloc;

	module->returnBlock = returnBlock;
	module->returnAlloc = nullptr;

	if (function->functionType->functionType.returnType->typeKind == AST::TypeKind::Void)
	{
		if (function->isEntryPoint)
		{
			module->returnAlloc = AllocateLocalVariable(llb, module, LLVMInt32TypeInContext(llb->llvmContext), "ret_val");
			LLVMBuildStore(module->builder, LLVMConstNull(LLVMInt32TypeInContext(llb->llvmContext)), module->returnAlloc);
		}
	}
	else
	{
		if (returnValueAsArg)
		{
			module->returnAlloc = LLVMGetParam(llvmValue, 0);
		}
		else
		{
			module->returnAlloc = AllocateLocalVariable(llb, module, returnType, "ret_val");
			LLVMBuildStore(module->builder, LLVMConstNull(returnType), module->returnAlloc);
		}
	}

	for (int i = 0; i < function->paramTypes.size; i++)
	{
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
			DebugInfoDeclareParameter(llb, module, argAlloc, i, function->paramTypes[i]->typeID, function->paramNames[i], function->paramTypes[i]->location);
		}

		function->paramVariables[i]->allocHandle = argAlloc;
	}

	if (strcmp(function->name, "main") == 0)
		__debugbreak();

	GenStatement(llb, module, function->body);

	LLVM_CALL(LLVMAppendExistingBasicBlock, llvmValue, returnBlock);

	if (!BlockHasBranched(llb, module))
		LLVM_CALL(LLVMBuildBr, module->builder, returnBlock);
	if (LLVM_CALL(LLVMGetInsertBlock, module->builder) != returnBlock)
		LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, returnBlock);

	// Emit return location
	if (module->hasDebugInfo)
	{
		DebugInfoEmitSourceLocation(llb, module, module->builder, function->endLocation);
	}

	if (function->functionType->functionType.returnType->typeKind == AST::TypeKind::Void)
	{
		if (function->isEntryPoint)
		{
			LLVMValueRef exitCode = LLVM_CALL(LLVMBuildLoad, module->builder, module->returnAlloc, "");
			LLVM_CALL(LLVMBuildRet, module->builder, exitCode);
		}
		else
		{
			LLVM_CALL(LLVMBuildRetVoid, module->builder);
		}
	}
	else
	{
		if (returnValueAsArg)
			LLVM_CALL(LLVMBuildRetVoid, module->builder);
		else
		{
			LLVMValueRef bitcastedAlloc = LLVMBuildBitCast(module->builder, module->returnAlloc, LLVMPointerType(functionReturnType, 0), "");
			LLVM_CALL(LLVMBuildRet, module->builder, LLVM_CALL(LLVMBuildLoad, module->builder, bitcastedAlloc, ""));
		}
	}

	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, parentBlock);

	module->returnBlock = parentReturnBlock;
	module->returnAlloc = parentReturnAlloc;

	if (module->hasDebugInfo)
	{
		DebugInfoEndFunction(llb, module, function);
	}
}

LLVMValueRef CallFunction(LLVMBackend* llb, SkModule* module, TypeID functionType, LLVMValueRef callee, LLVMValueRef* args, int numArgs)
{
	LLVMTypeRef returnType = GenTypeID(llb, module, functionType->functionType.returnType);
	LLVMTypeRef _returnType = CanPassByValue(llb, module, returnType);
	bool returnValueAsArg = !_returnType;
	LLVMValueRef returnValueAlloc = NULL;

	List<LLVMValueRef> arguments = CreateList<LLVMValueRef>(numArgs);

	if (returnValueAsArg)
	{
		returnValueAlloc = LLVMBuildAlloca(module->builder, returnType, "");
		arguments.add(returnValueAlloc);
	}

	for (int i = 0; i < numArgs; i++)
	{
		LLVMValueRef arg = args[i];
		LLVMTypeRef paramType = LLVMTypeOf(arg);

		if (LLVMTypeRef _paramType = CanPassByValue(llb, module, paramType))
		{
			if (paramType != _paramType)
			{
				arg = BitcastValue(llb, module, arg, _paramType);
			}
		}
		else
		{
			// Allocate argument in the caller stackframe
			LLVMValueRef argAlloc = LLVMBuildAlloca(module->builder, paramType, "");
			LLVM_CALL(LLVMBuildStore, module->builder, arg, argAlloc);
			arg = argAlloc;
		}

		arguments.add(arg);
	}

	LLVMValueRef callValue = LLVM_CALL(LLVMBuildCall, module->builder, callee, arguments.buffer, arguments.size, "");

	LLVMValueRef result = NULL;
	if (!returnValueAsArg)
	{
		if (returnType != _returnType)
		{
			callValue = BitcastValue(llb, module, callValue, _returnType);
		}
		result = callValue;
	}
	else
	{
		result = LLVM_CALL(LLVMBuildLoad, module->builder, returnValueAlloc, "");
	}

	DestroyList(arguments);

	return result;
}

LLVMTypeRef FunctionType(LLVMBackend* llb, SkModule* module, LLVMTypeRef returnType, LLVMTypeRef* paramTypes, int numParams, bool varArgs)
{
	// x64 calling convention

	LLVMTypeRef functionReturnType = NULL;
	List<LLVMTypeRef> functionParamTypes = CreateList<LLVMTypeRef>(numParams);

	if (LLVMTypeRef _functionReturnType = CanPassByValue(llb, module, returnType))
	{
		functionReturnType = _functionReturnType;
	}
	else
	{
		functionReturnType = LLVMVoidTypeInContext(llb->llvmContext);
		functionParamTypes.add(LLVMPointerType(returnType, 0));
	}

	for (int i = 0; i < numParams; i++)
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

	LLVMTypeRef functionType = LLVMFunctionType(functionReturnType, functionParamTypes.buffer, functionParamTypes.size, varArgs);
	DestroyList(functionParamTypes);

	return functionType;
}
