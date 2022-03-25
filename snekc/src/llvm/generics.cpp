#include "generics.h"

#include "semantics/Resolver.h"
#include "function.h"

#include "ast/File.h"


LLVMValueRef GenGenericFunctionInstance(LLVMBackend* llb, SkModule* module, AST::Function* function, List<LLVMTypeRef>& genericArgs, TypeID& functionType)
{
	LLVMValueRef llvmValue = GenFunctionHeader(llb, module, function);
	GenFunction(llb, module, function);

	return llvmValue;

	/*
	LLVMLinkage linkage = LLVMPrivateLinkage;

	LLVMTypeRef returnType = GenType(llb, module, function->returnType);
	int numParams = function->paramTypes.size;
	List<LLVMTypeRef> paramTypes = CreateList<LLVMTypeRef>(numParams);
	for (int i = 0; i < numParams; i++)
	{
		LLVMTypeRef paramType = GenType(llb, module, function->paramTypes[i]);
		paramTypes.add(paramType);
	}

	LLVMValueRef llvmValue = CreateFunction(llb, module, function->mangledName, returnType, paramTypes, function->varArgs, false, linkage, module->llvmModule);
	*/

	// TODO add to map
	//module->functionValues.emplace(decl, llvmValue);

	/*
	GenericData genericData = {};
	for (int i = 0; i < function->genericParams.size; i++)
	{
		genericData.types.emplace(function->genericParams[i], genericArgs[i]);
	}
	*/

	//module->genericData.push(std::move(genericData));

	//GenerateFunctionBody(llb, module, function, llvmValue);

	//module->genericData.pop();

	//return llvmValue;
}
