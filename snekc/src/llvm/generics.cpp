#include "generics.h"

#include "ast.h"
#include "function.h"


LLVMValueRef GenGenericFunctionInstance(LLVMBackend* llb, SkModule* module, AstFunction* function, List<LLVMTypeRef>& genericArgs, TypeID& functionType)
{
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

	// TODO add to map
	//module->functionValues.emplace(decl, llvmValue);

	GenericData genericData = {};
	for (int i = 0; i < function->genericParams.size; i++)
	{
		genericData.types.emplace(function->genericParams[i], genericArgs[i]);
	}

	module->genericData.push(std::move(genericData));

	// TODO resolve function

	GenerateFunctionBody(llb, module, function, llvmValue);

	module->genericData.pop();

	return llvmValue;
}
