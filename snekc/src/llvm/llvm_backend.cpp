#include "llvm_backend.h"

#include "snek.h"
#include "ast.h"
#include "log.h"
#include "debug.h"
#include "function.h"
#include "type.h"
#include "types.h"
#include "values.h"
#include "operators.h"
#include "debug_info.h"

#include <string.h>
#include <sstream>

#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/InstCombine.h>
#include <llvm-c/Transforms/AggressiveInstCombine.h>
#include <llvm-c/Transforms/Scalar.h>


#define MAX_FUNCTION_PARAMS 32


LLVMBackend* CreateLLVMBackend(SkContext* context)
{
	LLVMBackend* llb = new LLVMBackend;
	llb->context = context;
	llb->llvmContext = LLVMContextCreate();

	return llb;
}

void DestroyLLVMBackend(LLVMBackend* llb)
{
	delete llb;
}

static SkModule* CreateModule(AstFile* ast, bool hasDebugInfo)
{
	SkModule* module = new SkModule;
	module->ast = ast;
	module->hasDebugInfo = hasDebugInfo;

	module->debugScopes = CreateList<LLVMMetadataRef>();

	return module;
}

static LLVMTypeRef GenTypeID(LLVMBackend* llb, SkModule* module, TypeID type, AstType* ast = NULL);
static LLVMTypeRef GenType(LLVMBackend* llb, SkModule* module, AstType* type);
static LLVMValueRef GenExpression(LLVMBackend* llb, SkModule* module, AstExpression* expression);

static LLVMTypeRef GenStructHeader(LLVMBackend* llb, SkModule* module, AstStruct* decl);
static LLVMTypeRef GenClassHeader(LLVMBackend* llb, SkModule* module, AstClass* decl);

static LLVMTypeRef GenVoidType(LLVMBackend* llb, SkModule* module)
{
	return LLVM_CALL(LLVMVoidTypeInContext, llb->llvmContext);
}

static LLVMTypeRef GenIntegerType(LLVMBackend* llb, SkModule* module, TypeID type)
{
	return LLVM_CALL(LLVMIntTypeInContext, llb->llvmContext, type->integerType.bitWidth);
}

static LLVMTypeRef GenFPType(LLVMBackend* llb, SkModule* module, TypeID type)
{
	switch (type->fpType.precision)
	{
	case FP_PRECISION_HALF:
		return LLVM_CALL(LLVMHalfTypeInContext, llb->llvmContext);
	case FP_PRECISION_SINGLE:
		return LLVM_CALL(LLVMFloatTypeInContext, llb->llvmContext);
	case FP_PRECISION_DOUBLE:
		return LLVM_CALL(LLVMDoubleTypeInContext, llb->llvmContext);
	case FP_PRECISION_FP128:
		return LLVM_CALL(LLVMFP128TypeInContext, llb->llvmContext);
	default:
		SnekAssert(false);
		return NULL;
	}
}

static LLVMTypeRef GenBoolType(LLVMBackend* llb, SkModule* module)
{
	return LLVM_CALL(LLVMInt1TypeInContext, llb->llvmContext);
}

static LLVMTypeRef GenStructType(LLVMBackend* llb, SkModule* module, TypeID type, AstNamedType* ast)
{
	/*
	int numFields = type->structType.numFields;
	LLVMTypeRef* fieldTypes = new LLVMTypeRef[numFields];
	for (int i = 0; i < numFields; i++)
	{
		fieldTypes[i] = GenTypeID(llb, module, type->structType.fieldTypes[i]);
	}

	LLVMTypeRef structType = LLVM_CALL(LLVMStructTypeInContext, llb->llvmContext, fieldTypes, numFields, false);

	return structType;
	*/

	LLVMTypeRef structType = (LLVMTypeRef)type->structType.declaration->typeHandle;

	return structType;
}

static LLVMTypeRef GenClassType(LLVMBackend* llb, SkModule* module, TypeID type, AstNamedType* ast)
{
	/*
	int numFields = type->structType.numFields;
	LLVMTypeRef* fieldTypes = new LLVMTypeRef[numFields];
	for (int i = 0; i < numFields; i++)
	{
		fieldTypes[i] = GenTypeID(llb, module, type->structType.fieldTypes[i]);
	}

	LLVMTypeRef structType = LLVM_CALL(LLVMStructTypeInContext, llb->llvmContext, fieldTypes, numFields, false);
	LLVMTypeRef classType = LLVM_CALL(LLVMPointerType, structType, 0);

	return classType;
	*/

	LLVMTypeRef structType = (LLVMTypeRef)type->classType.declaration->typeHandle;
	LLVMTypeRef classType = LLVM_CALL(LLVMPointerType, structType, 0);

	return classType;
}

static LLVMTypeRef GenAliasType(LLVMBackend* llb, SkModule* module, TypeID type, AstNamedType* ast)
{
	if (ast)
	{
		if (ast->typedefDecl)
			return GenTypeID(llb, module, type->aliasType.alias, ast->typedefDecl->alias);
		else if (ast->enumDecl)
			return GenTypeID(llb, module, type->aliasType.alias, ast->enumDecl->alias);
		else
		{
			SnekAssert(false);
			return NULL;
		}
	}
	else
	{
		return GenTypeID(llb, module, type->aliasType.alias, NULL);
	}
}

static LLVMTypeRef GenPointerType(LLVMBackend* llb, SkModule* module, TypeID type, AstPointerType* ast)
{
	LLVMTypeRef elementType = GenTypeID(llb, module, type->pointerType.elementType, ast ? ast->elementType : NULL);
	if (LLVMGetTypeKind(elementType) == LLVMVoidTypeKind)
		return LLVM_CALL(LLVMPointerType, LLVMInt8TypeInContext(llb->llvmContext), 0);
	else
		return LLVM_CALL(LLVMPointerType, elementType, 0);
}

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

static LLVMTypeRef GenFunctionType(LLVMBackend* llb, SkModule* module, TypeID type, AstFunctionType* ast)
{
	// x64 calling convention

	LLVMTypeRef functionReturnType = NULL;
	List<LLVMTypeRef> functionParamTypes = CreateList<LLVMTypeRef>();

	LLVMTypeRef returnType = GenTypeID(llb, module, type->functionType.returnType, ast ? ast->returnType : NULL);
	if (LLVMTypeRef _functionReturnType = CanPassByValue(llb, module, returnType))
	{
		functionReturnType = _functionReturnType;
	}
	else
	{
		functionReturnType = LLVMVoidTypeInContext(llb->llvmContext);
		functionParamTypes.add(LLVMPointerType(returnType, 0));
	}

	int numParams = type->functionType.numParams;
	for (int i = 0; i < numParams; i++)
	{
		LLVMTypeRef paramType = GenTypeID(llb, module, type->functionType.paramTypes[i], ast ? ast->paramTypes[i] : NULL);
		if (LLVMTypeRef _paramType = CanPassByValue(llb, module, paramType))
		{
			functionParamTypes.add(_paramType);
		}
		else
		{
			functionParamTypes.add(LLVMPointerType(paramType, 0));
		}
	}

	LLVMTypeRef functionType = LLVMFunctionType(functionReturnType, functionParamTypes.buffer, functionParamTypes.size, type->functionType.varArgs);
	DestroyList(functionParamTypes);

	return LLVMPointerType(functionType, 0);
}

static LLVMTypeRef GenStringType(LLVMBackend* llb, SkModule* module, TypeID type)
{
	return GetStringType(llb);
}

static LLVMTypeRef GenArrayType(LLVMBackend* llb, SkModule* module, TypeID type, AstArrayType* ast)
{
	LLVMTypeRef elementType = GenTypeID(llb, module, type->arrayType.elementType, ast ? ast->elementType : NULL);
	/*
	if (type->arrayType.length)
	{
		LLVMValueRef arrayLength = LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), type->arrayType.length, false);
		elementType = LLVM_CALL(LLVMArrayType, GenTypeID(llb, module, type->arrayType.elementType, ast ? ast->elementType : NULL), (int)LLVMConstIntGetZExtValue(arrayLength));
	}
	else
	{

	}
	*/

	return GetArrayType(llb, elementType, type->arrayType.length);
}

static LLVMTypeRef GenTypeID(LLVMBackend* llb, SkModule* module, TypeID type, AstType* ast)
{
	switch (type->typeKind)
	{
	case TYPE_KIND_VOID:
		return GenVoidType(llb, module);
	case TYPE_KIND_INTEGER:
		return GenIntegerType(llb, module, type);
	case TYPE_KIND_FP:
		return GenFPType(llb, module, type);
	case TYPE_KIND_BOOL:
		return GenBoolType(llb, module);
	case TYPE_KIND_STRUCT:
		return GenStructType(llb, module, type, (AstNamedType*)ast);
	case TYPE_KIND_CLASS:
		return GenClassType(llb, module, type, (AstNamedType*)ast);
	case TYPE_KIND_ALIAS:
		return GenAliasType(llb, module, type, (AstNamedType*)ast);
	case TYPE_KIND_POINTER:
		return GenPointerType(llb, module, type, (AstPointerType*)ast);
	case TYPE_KIND_FUNCTION:
		return GenFunctionType(llb, module, type, (AstFunctionType*)ast);

	case TYPE_KIND_ARRAY:
		return GenArrayType(llb, module, type, (AstArrayType*)ast);
	case TYPE_KIND_STRING:
		return GenStringType(llb, module, type);

	default:
		SnekAssert(false);
		return NULL;
	}
}

static LLVMTypeRef GenType(LLVMBackend* llb, SkModule* module, AstType* type)
{
	return GenTypeID(llb, module, type->typeID, type);
}

static LLVMValueRef GenExpression(LLVMBackend* llb, SkModule* module, AstExpression* expression);
static LLVMValueRef GenFunctionHeader(LLVMBackend* llb, SkModule* module, AstFunction* decl);
static LLVMValueRef GenGlobalHeader(LLVMBackend* llb, SkModule* module, AstGlobal* global);
static LLVMValueRef GenClassMethodHeader(LLVMBackend* llb, SkModule* module, AstFunction* method, AstClass* classDecl);

LLVMValueRef GetRValue(LLVMBackend* llb, SkModule* module, LLVMValueRef value, bool lvalue)
{
	if (lvalue)
	{
		if (LLVM_CALL(LLVMIsAGlobalVariable, value) && LLVM_CALL(LLVMIsGlobalConstant, value))
		{
			return LLVM_CALL(LLVMGetInitializer, value);
		}
		else
		{
			return LLVM_CALL(LLVMBuildLoad, module->builder, value, "");
		}
	}
	else
	{
		return value;
	}
}

static LLVMValueRef GenIntegerLiteral(LLVMBackend* llb, SkModule* module, AstIntegerLiteral* expression)
{
	LLVMTypeRef type = GenTypeID(llb, module, expression->type);
	return LLVM_CALL(LLVMConstInt, type, expression->value, expression->value < 0);
}

static LLVMValueRef GenFPLiteral(LLVMBackend* llb, SkModule* module, AstFPLiteral* expression)
{
	return LLVM_CALL(LLVMConstReal, LLVMFloatTypeInContext(llb->llvmContext), expression->value);
}

static LLVMValueRef GenBoolLiteral(LLVMBackend* llb, SkModule* module, AstBoolLiteral* expression)
{
	return LLVM_CALL(LLVMConstInt, LLVMInt1TypeInContext(llb->llvmContext), expression->value ? 1 : 0, false);
}

static LLVMValueRef GenCharacterLiteral(LLVMBackend* llb, SkModule* module, AstCharacterLiteral* expression)
{
	return LLVM_CALL(LLVMConstInt, LLVMInt8TypeInContext(llb->llvmContext), expression->value, false);
}

static LLVMValueRef GenNullLiteral(LLVMBackend* llb, SkModule* module, AstNullLiteral* expression)
{
	return LLVM_CALL(LLVMConstNull, LLVMPointerType(LLVMInt8TypeInContext(llb->llvmContext), 0));
}

static LLVMValueRef GenStringLiteral(LLVMBackend* llb, SkModule* module, AstStringLiteral* expression)
{
	return CreateStringLiteral(llb, module, expression->value);
}

static LLVMValueRef GenStructLiteral(LLVMBackend* llb, SkModule* module, AstStructLiteral* expression)
{
	LLVMTypeRef llvmType = GenType(llb, module, expression->structType);

	if (IsConstant(expression))
	{
		int numFields = expression->type->structType.numFields;
		LLVMValueRef* values = new LLVMValueRef[numFields];

		for (int i = 0; i < numFields; i++)
		{
			LLVMTypeRef fieldType = GenTypeID(llb, module, expression->structType->typeID->structType.fieldTypes[i]);
			if (i < expression->values.size)
			{
				LLVMValueRef field = GetRValue(llb, module, GenExpression(llb, module, expression->values[i]), expression->values[i]->lvalue);
				if (IsConstant(expression->values[i]))
					field = ConstCastValue(llb, module, field, fieldType, expression->values[i]->type, expression->structType->typeID->structType.fieldTypes[i]);
				else
					field = CastValue(llb, module, field, fieldType, expression->values[i]->type, expression->structType->typeID->structType.fieldTypes[i]);
				values[i] = field;
				//LLVMValueRef fieldAlloc = LLVMBuildStructGEP(module->builder, alloc, i, "");
				//LLVMBuildStore(module->builder, field, fieldAlloc);

				//return LLVMBuildLoad(module->builder, alloc, "");
			}
			else
			{
				values[i] = LLVMConstNull(fieldType);
			}
		}

		LLVMValueRef value = LLVMConstNamedStruct(llvmType, values, numFields);
		delete values;

		return value;
	}
	else
	{
		LLVMValueRef alloc = LLVMBuildAlloca(module->builder, llvmType, "");
		for (int i = 0; i < expression->values.size; i++)
		{
			LLVMValueRef field = GetRValue(llb, module, GenExpression(llb, module, expression->values[i]), expression->values[i]->lvalue);
			LLVMTypeRef fieldType = GenTypeID(llb, module, expression->structType->typeID->structType.fieldTypes[i]);
			field = CastValue(llb, module, field, fieldType, expression->values[i]->type, expression->structType->typeID->structType.fieldTypes[i]);
			LLVMValueRef fieldAlloc = LLVMBuildStructGEP(module->builder, alloc, i, "");
			LLVMBuildStore(module->builder, field, fieldAlloc);
		}
		return LLVMBuildLoad(module->builder, alloc, "");
	}

	int numValues = expression->values.size;
	LLVMValueRef* values = new LLVMValueRef[numValues];
	for (int i = 0; i < numValues; i++)
	{
		values[i] = GenExpression(llb, module, expression->values[i]);
	}

	return LLVMConstStructInContext(llb->llvmContext, values, numValues, false);
}

static LLVMValueRef GenIdentifier(LLVMBackend* llb, SkModule* module, AstIdentifier* expression)
{
	if (expression->variable)
	{
		LLVMValueRef value = NULL;
		/*
		if (expression->variable->isConstant)
		{
			value = LLVMGetInitializer((LLVMValueRef)expression->variable->allocHandle);
		}
		else
		*/
		//{
		if (expression->module == expression->variable->module || expression->variable->isConstant)
		{
			value = (LLVMValueRef)expression->variable->allocHandle;
		}
		else
		{
			// Import global
			auto it = module->globalValues.find(expression->variable->globalDecl);
			if (it != module->globalValues.end())
				value = it->second;
			else
			{
				value = GenGlobalHeader(llb, module, expression->variable->globalDecl);
			}
		}
		//}
		return value;
	}
	else if (expression->function)
	{
		if (expression->function->module == expression->module)
			return module->functionValues[expression->function];
		else
			return GenFunctionHeader(llb, module, expression->function);
		//LLVMValueRef func = (LLVMValueRef)expression->function->valueHandle;
		//return func;
	}
	else if (expression->enumValue)
	{
		return (LLVMValueRef)expression->enumValue->valueHandle;
	}
	else if (expression->exprdefValue)
	{
		return GenExpression(llb, module, expression->exprdefValue);
	}
	else
	{
		SnekAssert(false);
		return NULL;
	}
}

static LLVMValueRef GenCompoundExpression(LLVMBackend* llb, SkModule* module, AstCompoundExpression* expression)
{
	return GenExpression(llb, module, expression->value);
}

static LLVMValueRef GenFunctionCall(LLVMBackend* llb, SkModule* module, AstFuncCall* expression)
{
	LLVMValueRef callee = NULL;
	TypeID functionType = NULL;

	if (expression->function)
	{
		if (expression->function->module == expression->module)
		{
			//callee = (LLVMValueRef)expression->function->valueHandle;
			// TODO check this
			callee = module->functionValues[expression->function];
		}
		else
		{
			// Import function
			auto it = module->functionValues.find(expression->function);
			if (it != module->functionValues.end())
				callee = it->second;
			else
			{
				if (expression->methodInstance)
					callee = GenClassMethodHeader(llb, module, expression->function, expression->function->instanceType->classType.declaration);
				else
					callee = GenFunctionHeader(llb, module, expression->function);
			}
		}
		functionType = expression->function->type;
	}
	else if (expression->calleeExpr)
	{
		callee = GenExpression(llb, module, expression->calleeExpr);
		callee = GetRValue(llb, module, callee, expression->calleeExpr->lvalue);
		functionType = expression->calleeExpr->type;
		while (functionType->typeKind == TYPE_KIND_ALIAS)
			functionType = functionType->aliasType.alias;
	}
	else
	{
		SnekAssert(false);
		return NULL;
	}


	LLVMTypeRef llvmFunctionType = LLVM_CALL(LLVMGetElementType, LLVMTypeOf(callee));
	SnekAssert(LLVMGetTypeKind(llvmFunctionType) == LLVMFunctionTypeKind);

	//int numParams = LLVM_CALL(LLVMCountParamTypes, llvmFunctionType);
	int numParams = functionType->functionType.numParams;
	LLVMTypeRef returnType = GenTypeID(llb, module, functionType->functionType.returnType);
	//LLVMTypeRef returnType = LLVMGetReturnType(llvmFunctionType);
	LLVMTypeRef* paramTypes = new LLVMTypeRef[numParams];
	for (int i = 0; i < numParams; i++)
		paramTypes[i] = GenTypeID(llb, module, functionType->functionType.paramTypes[i]);
	//LLVM_CALL(LLVMGetParamTypes, LLVMGetElementType(LLVMTypeOf(callee)), paramTypes);
	bool returnValueAsArg = !CanPassByValue(llb, module, returnType);

	int numArgs = expression->arguments.size;
	List<LLVMValueRef> arguments = CreateList<LLVMValueRef>();
	LLVMValueRef returnValueAlloc = NULL;

	if (returnValueAsArg)
	{
		returnValueAlloc = AllocateLocalVariable(llb, module, returnType, "");
		arguments.add(returnValueAlloc);
	}

	if (expression->isMethodCall)
	{
		LLVMValueRef instance = GenExpression(llb, module, expression->methodInstance);
		// TODO dont gen this twice
		SnekAssert(expression->methodInstance->lvalue || expression->methodInstance->type->typeKind == TYPE_KIND_POINTER);
		//arguments.add(LLVM_CALL(LLVMBuildLoad, module->builder, instance, ""));
		arguments.add(instance);
	}

	for (int i = 0; i < numArgs; i++)
	{
		LLVMValueRef arg = GenExpression(llb, module, expression->arguments[i]);
		//LLVMTypeRef paramType = paramTypes[i + returnValueAsArg ? 1 : 0];
		if (i < numParams)
		{
			LLVMTypeRef paramType = GenTypeID(llb, module, functionType->functionType.paramTypes[i]);
			arg = ConvertArgumentValue(llb, module, arg, paramType, expression->arguments[i]->type, functionType->functionType.paramTypes[i], expression->arguments[i]->lvalue, IsConstant(expression->arguments[i]));

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
				LLVMValueRef argAlloc = AllocateLocalVariable(llb, module, paramType, "");
				LLVM_CALL(LLVMBuildStore, module->builder, arg, argAlloc);
				arg = argAlloc;
			}
		}
		else
		{
			arg = GetRValue(llb, module, arg, expression->arguments[i]->lvalue);
			if (expression->arguments[i]->type->typeKind == TYPE_KIND_FP)
				arg = CastValue(llb, module, arg, LLVMDoubleTypeInContext(llb->llvmContext), expression->arguments[i]->type, GetFPType(FP_PRECISION_DOUBLE));
		}

		arguments.add(arg);
	}

	LLVMValueRef callValue = LLVM_CALL(LLVMBuildCall, module->builder, callee, arguments.buffer, arguments.size, "");

	LLVMValueRef result = NULL;
	if (LLVMTypeRef _returnType = CanPassByValue(llb, module, returnType))
	{
		if (returnType != _returnType)
		{
			callValue = BitcastValue(llb, module, callValue, returnType);
		}
		result = callValue;
	}
	else
	{
		result = LLVM_CALL(LLVMBuildLoad, module->builder, returnValueAlloc, "");
	}

	delete paramTypes;
	DestroyList(arguments);

	return result;
}

static LLVMValueRef GenSubscriptOperator(LLVMBackend* llb, SkModule* module, AstSubscriptOperator* expression)
{
	LLVMValueRef operand = GenExpression(llb, module, expression->operand);

	if (expression->operand->type->typeKind == TYPE_KIND_ARRAY ||
		expression->operand->type->typeKind == TYPE_KIND_POINTER && expression->operand->type->pointerType.elementType->typeKind == TYPE_KIND_ARRAY)
	{
		SnekAssert(expression->lvalue || expression->operand->type->typeKind == TYPE_KIND_POINTER);
		SnekAssert(LLVMGetTypeKind(LLVMTypeOf(operand)) == LLVMPointerTypeKind);
		SnekAssert(expression->arguments.size == 1);

		if (expression->lvalue && expression->operand->type->typeKind == TYPE_KIND_POINTER)
			operand = LLVMBuildLoad(module->builder, operand, "");

		LLVMValueRef index = GenExpression(llb, module, expression->arguments[0]);
		index = GetRValue(llb, module, index, expression->arguments[0]->lvalue);
		LLVMValueRef element = GetArrayElement(llb, module, operand, index);

		return element;
	}
	else if (expression->operand->type->typeKind == TYPE_KIND_STRING)
	{
		operand = GetRValue(llb, module, operand, expression->operand->lvalue);

		SnekAssert(expression->arguments.size == 1);

		LLVMValueRef index = GenExpression(llb, module, expression->arguments[0]);
		index = GetRValue(llb, module, index, expression->arguments[0]->lvalue);
		LLVMValueRef element = GetStringElement(llb, module, operand, index);

		return element;
	}
	else if (expression->operand->type->typeKind == TYPE_KIND_POINTER)
	{
		operand = GetRValue(llb, module, operand, expression->operand->lvalue);

		SnekAssert(expression->arguments.size == 1);

		LLVMValueRef index = GenExpression(llb, module, expression->arguments[0]);
		index = GetRValue(llb, module, index, expression->arguments[0]->lvalue);
		LLVMValueRef element = LLVM_CALL(LLVMBuildGEP, module->builder, operand, (LLVMValueRef*)&index, 1, "");

		return element;
	}

	return NULL;
}

static LLVMValueRef GenDotOperator(LLVMBackend* llb, SkModule* module, AstDotOperator* expression)
{
	LLVMValueRef operand = GenExpression(llb, module, expression->operand);

	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(operand)) == LLVMPointerTypeKind);
	//if (LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(operand))) == LLVMPointerTypeKind)
	//	operand = LLVM_CALL(LLVMBuildLoad, module->builder, operand, "");

	TypeID operandType = expression->operand->type;
	if (operandType->typeKind == TYPE_KIND_POINTER)
	{
		operandType = operandType->pointerType.elementType;
		if (expression->operand->lvalue)
		{
			operand = LLVM_CALL(LLVMBuildLoad, module->builder, operand, "");
		}
	}

	while (operandType->typeKind == TYPE_KIND_ALIAS)
		operandType = operandType->aliasType.alias;

	if (operandType->typeKind == TYPE_KIND_STRUCT)
	{
		if (expression->classMethod)
		{
			expression->methodInstance = operand;
			return (LLVMValueRef)expression->classMethod->valueHandle;
		}
		else
		{
			return GetStructMember(llb, module, operand, expression->structField->index, expression->structField->name);
		}
	}
	else if (operandType->typeKind == TYPE_KIND_CLASS)
	{
		return GetClassMember(llb, module, operand, expression->classField->index, expression->classField->name);
	}
	else if (operandType->typeKind == TYPE_KIND_ARRAY)
	{
		//operand = GetRValue(llb, module, operand, expression->operand->lvalue);
		SnekAssert(expression->operand->lvalue || expression->operand->type->typeKind == TYPE_KIND_POINTER);
		if (expression->arrayField == 0)
			return GetArrayLength(llb, module, operand);
		else if (expression->arrayField == 1)
			return GetArrayBuffer(llb, module, operand);
		else
		{
			SnekAssert(false);
			return NULL;
		}
	}
	else if (operandType->typeKind == TYPE_KIND_STRING)
	{
		operand = GetRValue(llb, module, operand, expression->operand->lvalue);
		if (expression->stringField == 0)
			return GetStringLength(llb, module, operand);
		else if (expression->stringField == 1)
			return GetStringBuffer(llb, module, operand);
		else
		{
			SnekAssert(false);
			return NULL;
		}
	}

	SnekAssert(false);
	return NULL;
}

static LLVMValueRef GenCast(LLVMBackend* llb, SkModule* module, AstCast* expression)
{
	LLVMValueRef value = GenExpression(llb, module, expression->value);
	value = GetRValue(llb, module, value, expression->value->lvalue);
	LLVMTypeRef type = GenType(llb, module, expression->castType);
	return CastValue(llb, module, value, type, expression->value->type, expression->castType->typeID);
}

static LLVMValueRef GenClassConstructorHeader(LLVMBackend* llb, SkModule* module, AstFunction* constructor, AstClass* classDecl);

static LLVMValueRef GenSizeof(LLVMBackend* llb, SkModule* module, AstSizeof* expression)
{
	LLVMTypeRef llvmType = GenType(llb, module, expression->sizedType);
	if (expression->sizedType->typeKind == TYPE_KIND_CLASS)
	{
		llvmType = LLVM_CALL(LLVMGetElementType, llvmType);
	}

	return LLVM_CALL(LLVMSizeOf, llvmType);
}

static LLVMValueRef GenMalloc(LLVMBackend* llb, SkModule* module, AstMalloc* expression)
{
	LLVMTypeRef llvmType = GenType(llb, module, expression->mallocType);
	if (expression->mallocType->typeKind == TYPE_KIND_CLASS || expression->mallocType->typeKind == TYPE_KIND_STRING) // If reference type
	{
		llvmType = LLVMGetElementType(llvmType);
	}

	LLVMValueRef alloc = NULL;
	if (expression->count)
	{
		LLVMValueRef count = GetRValue(llb, module, GenExpression(llb, module, expression->count), expression->count->lvalue);
		alloc = LLVM_CALL(LLVMBuildArrayMalloc, module->builder, llvmType, count, "");
	}
	else
	{
		alloc = LLVM_CALL(LLVMBuildMalloc, module->builder, llvmType, "");
	}

	if (expression->mallocType->typeKind == TYPE_KIND_CLASS && expression->hasArguments)
	{
		AstClass* declaration = expression->mallocType->typeID->classType.declaration;
		if (AstFunction* constructor = declaration->constructor)
		{
			LLVMValueRef callee = NULL;
			if (constructor->module == expression->module)
			{
				//callee = (LLVMValueRef)expression->function->valueHandle;
				callee = (LLVMValueRef)constructor->valueHandle;
			}
			else
			{
				// Import function
				auto it = module->functionValues.find(constructor);
				if (it != module->functionValues.end())
					callee = it->second;
				else
					callee = GenClassConstructorHeader(llb, module, constructor, declaration);
			}

			int numParams = LLVM_CALL(LLVMCountParamTypes, LLVMGetElementType(LLVMTypeOf(callee)));
			LLVMTypeRef* paramTypes = new LLVMTypeRef[numParams];
			LLVM_CALL(LLVMGetParamTypes, LLVMGetElementType(LLVMTypeOf(callee)), paramTypes);

			int numArgs = expression->arguments.size + 1;
			LLVMValueRef* arguments = new LLVMValueRef[numArgs];

			arguments[0] = alloc;
			for (int i = 1; i < numArgs; i++)
			{
				AstExpression* argExpr = expression->arguments[i - 1];
				TypeID paramTypeID = constructor->paramTypes[i - 1]->typeID;

				LLVMValueRef arg = GenExpression(llb, module, argExpr);
				if (i < numParams)
				{
					LLVMTypeRef paramType = paramTypes[i];
					arg = ConvertArgumentValue(llb, module, arg, paramType, argExpr->type, paramTypeID, argExpr->lvalue, LLVMIsConstant(arg));
				}
				else
				{
					arg = GetRValue(llb, module, arg, argExpr->lvalue);
					if (argExpr->type->typeKind == TYPE_KIND_FP)
						arg = CastValue(llb, module, arg, LLVMDoubleTypeInContext(llb->llvmContext), argExpr->type, GetFPType(FP_PRECISION_DOUBLE));
				}
				arguments[i] = arg;
			}

			LLVMValueRef result = LLVM_CALL(LLVMBuildCall, module->builder, callee, arguments, numArgs, "");

			delete paramTypes, arguments;

			return result;
		}
	}

	return alloc;
}

static LLVMValueRef GenUnaryOperator(LLVMBackend* llb, SkModule* module, AstUnaryOperator* expression)
{
	LLVMValueRef operand = GenExpression(llb, module, expression->operand);

	switch (expression->operatorType)
	{
	case UNARY_OPERATOR_NOT: return OperatorNot(llb, module, operand, expression->operand->type, expression->operand->lvalue);
	case UNARY_OPERATOR_NEGATE: return OperatorNegate(llb, module, operand, expression->operand->type, expression->operand->lvalue);
	case UNARY_OPERATOR_REFERENCE: return OperatorReference(llb, module, operand, expression->operand->type, expression->operand->lvalue);
	case UNARY_OPERATOR_DEREFERENCE: return OperatorDereference(llb, module, operand, expression->operand->type, expression->operand->lvalue);

	case UNARY_OPERATOR_INCREMENT: return OperatorIncrement(llb, module, operand, expression->operand->type, expression->operand->lvalue, expression->position);
	case UNARY_OPERATOR_DECREMENT: return OperatorDecrement(llb, module, operand, expression->operand->type, expression->operand->lvalue, expression->position);

	default:
		SnekAssert(false);
		return NULL;
	}
}

static LLVMValueRef GenBinaryOperator(LLVMBackend* llb, SkModule* module, AstBinaryOperator* expression)
{
	LLVMValueRef left = GenExpression(llb, module, expression->left);
	LLVMValueRef right = GenExpression(llb, module, expression->right);

	switch (expression->operatorType)
	{
	case BINARY_OPERATOR_ADD: return OperatorAdd(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_SUB: return OperatorSub(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_MUL: return OperatorMul(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_DIV: return OperatorDiv(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_MOD: return OperatorMod(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);

	case BINARY_OPERATOR_EQ: return OperatorEQ(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_NE: return OperatorNE(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_LT: return OperatorLT(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_GT: return OperatorGT(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_LE: return OperatorLE(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_GE: return OperatorGE(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_AND: return OperatorAnd(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_OR: return OperatorOr(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_BITWISE_AND: return OperatorBWAnd(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_BITWISE_OR: return OperatorBWOr(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_BITWISE_XOR: return OperatorBWXor(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_BITSHIFT_LEFT: return OperatorBSLeft(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_BITSHIFT_RIGHT: return OperatorBSRight(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);

	case BINARY_OPERATOR_ASSIGN: return OperatorAssign(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue, IsLiteral(expression->right));
	case BINARY_OPERATOR_ADD_ASSIGN: return OperatorAddAssign(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_SUB_ASSIGN: return OperatorSubAssign(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_MUL_ASSIGN: return OperatorMulAssign(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_DIV_ASSIGN: return OperatorDivAssign(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_MOD_ASSIGN: return OperatorModAssign(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_BITSHIFT_LEFT_ASSIGN: return OperatorBSLeftAssign(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_BITSHIFT_RIGHT_ASSIGN: return OperatorBSRightAssign(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_BITWISE_AND_ASSIGN: return OperatorBWAndAssign(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_BITWISE_OR_ASSIGN: return OperatorBWOrAssign(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);
	case BINARY_OPERATOR_BITWISE_XOR_ASSIGN: return OperatorBWXorAssign(llb, module, left, right, expression->left->type, expression->right->type, expression->left->lvalue, expression->right->lvalue);

	default:
		SnekAssert(false);
		return NULL;
	}

	return NULL;
}

static LLVMValueRef GenTernaryOperator(LLVMBackend* llb, SkModule* module, AstTernaryOperator* expression)
{
	return OperatorTernary(llb, module,
		GenExpression(llb, module, expression->condition),
		GenExpression(llb, module, expression->thenValue),
		GenExpression(llb, module, expression->elseValue),
		expression->condition->type,
		expression->thenValue->type,
		expression->elseValue->type,
		expression->condition->lvalue,
		expression->thenValue->lvalue,
		expression->elseValue->lvalue
	);
}

static LLVMValueRef GenExpression(LLVMBackend* llb, SkModule* module, AstExpression* expression)
{
	switch (expression->exprKind)
	{
	case EXPR_KIND_INTEGER_LITERAL:
		return GenIntegerLiteral(llb, module, (AstIntegerLiteral*)expression);
	case EXPR_KIND_FP_LITERAL:
		return GenFPLiteral(llb, module, (AstFPLiteral*)expression);
	case EXPR_KIND_BOOL_LITERAL:
		return GenBoolLiteral(llb, module, (AstBoolLiteral*)expression);
	case EXPR_KIND_CHARACTER_LITERAL:
		return GenCharacterLiteral(llb, module, (AstCharacterLiteral*)expression);
	case EXPR_KIND_NULL_LITERAL:
		return GenNullLiteral(llb, module, (AstNullLiteral*)expression);
	case EXPR_KIND_STRING_LITERAL:
		return GenStringLiteral(llb, module, (AstStringLiteral*)expression);
	case EXPR_KIND_STRUCT_LITERAL:
		return GenStructLiteral(llb, module, (AstStructLiteral*)expression);
	case EXPR_KIND_IDENTIFIER:
		return GenIdentifier(llb, module, (AstIdentifier*)expression);
	case EXPR_KIND_COMPOUND:
		return GenCompoundExpression(llb, module, (AstCompoundExpression*)expression);

	case EXPR_KIND_FUNC_CALL:
		return GenFunctionCall(llb, module, (AstFuncCall*)expression);
	case EXPR_KIND_SUBSCRIPT_OPERATOR:
		return GenSubscriptOperator(llb, module, (AstSubscriptOperator*)expression);
	case EXPR_KIND_DOT_OPERATOR:
		return GenDotOperator(llb, module, (AstDotOperator*)expression);
	case EXPR_KIND_CAST:
		return GenCast(llb, module, (AstCast*)expression);
	case EXPR_KIND_SIZEOF:
		return GenSizeof(llb, module, (AstSizeof*)expression);
	case EXPR_KIND_MALLOC:
		return GenMalloc(llb, module, (AstMalloc*)expression);

	case EXPR_KIND_UNARY_OPERATOR:
		return GenUnaryOperator(llb, module, (AstUnaryOperator*)expression);
	case EXPR_KIND_BINARY_OPERATOR:
		return GenBinaryOperator(llb, module, (AstBinaryOperator*)expression);
	case EXPR_KIND_TERNARY_OPERATOR:
		return GenTernaryOperator(llb, module, (AstTernaryOperator*)expression);

	default:
		SnekAssert(false);
		return NULL;
	}
}

static void GenStatement(LLVMBackend* llb, SkModule* module, AstStatement* statement);

static bool BlockHasBranched(LLVMBackend* llb, SkModule* module)
{
	LLVMBasicBlockRef block = LLVM_CALL(LLVMGetInsertBlock, module->builder);
	if (LLVMValueRef lastInst = LLVM_CALL(LLVMGetLastInstruction, block)) {
		LLVMOpcode opCode = LLVM_CALL(LLVMGetInstructionOpcode, lastInst);
		return opCode == LLVMBr || opCode == LLVMRet;
	}
	return false;
}

static void GenCompoundStatement(LLVMBackend* llb, SkModule* module, AstCompoundStatement* statement)
{
	for (int i = 0; i < statement->statements.size; i++)
	{
		GenStatement(llb, module, statement->statements[i]);
	}
}

static void GenVarDeclStatement(LLVMBackend* llb, SkModule* module, AstVarDeclStatement* statement)
{
	LLVMTypeRef type = GenType(llb, module, statement->type);
	for (int i = 0; i < statement->declarators.size; i++)
	{
		AstVarDeclarator* declarator = &statement->declarators[i];

		const char* name = declarator->name;
		LLVMValueRef alloc = AllocateLocalVariable(llb, module, type, name);
		declarator->variable->allocHandle = alloc;

		if (declarator->value)
		{
			LLVMValueRef value = GenExpression(llb, module, declarator->value);
			value = ConvertAssignValue(llb, module, value, type, declarator->value->type, statement->type->typeID, declarator->value->lvalue, LLVMIsConstant(value));
			LLVM_CALL(LLVMBuildStore, module->builder, value, alloc);
		}

		if (module->hasDebugInfo)
		{
			DebugInfoDeclareVariable(llb, module, alloc, statement->type->typeID, name, statement->inputState.line, statement->inputState.col);
		}

		declarator->variable->allocHandle = alloc;
	}
}

static void GenExprStatement(LLVMBackend* llb, SkModule* module, AstExprStatement* statement)
{
	LLVMValueRef value = GenExpression(llb, module, statement->expr);
}

static void GenIfStatement(LLVMBackend* llb, SkModule* module, AstIfStatement* statement)
{
	LLVMValueRef condition = GenExpression(llb, module, statement->condition);
	condition = GetRValue(llb, module, condition, statement->condition->lvalue);
	condition = LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntNE, condition, LLVMConstNull(LLVMTypeOf(condition)), "");

	LLVMValueRef llvmFunction = module->functionValues[module->currentFunction];
	LLVMBasicBlockRef beforeBlock = LLVMGetInsertBlock(module->builder);
	LLVMBasicBlockRef thenBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "if.then");
	LLVMBasicBlockRef elseBlock = NULL;
	LLVMBasicBlockRef mergeBlock = LLVMCreateBasicBlockInContext(llb->llvmContext, "if.merge");

	if (statement->elseStatement)
	{
		elseBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "if.else");
		LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, beforeBlock);
		LLVM_CALL(LLVMBuildCondBr, module->builder, condition, thenBlock, elseBlock);
	}
	else
	{
		LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, beforeBlock);
		LLVM_CALL(LLVMBuildCondBr, module->builder, condition, thenBlock, mergeBlock);
	}

	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, thenBlock);
	GenStatement(llb, module, statement->thenStatement);
	//irCodeblock(irGen, ifStatement->thenBlock);
	if (!BlockHasBranched(llb, module)) {
		//irDeferredStatements(irGen, ifStatement->thenBlock->scope);
		LLVM_CALL(LLVMBuildBr, module->builder, mergeBlock);
	}

	if (statement->elseStatement) {
		LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, elseBlock);
		GenStatement(llb, module, statement->elseStatement);
		//irCodeblock(irGen, ifStatement->elseBlock);
		if (!BlockHasBranched(llb, module)) {
			//irDeferredStatements(irGen, ifStatement->elseBlock->scope);
			LLVM_CALL(LLVMBuildBr, module->builder, mergeBlock);
		}
	}

	LLVM_CALL(LLVMAppendExistingBasicBlock, llvmFunction, mergeBlock);
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, mergeBlock);
}

static void GenWhileLoop(LLVMBackend* llb, SkModule* module, AstWhileLoop* statement)
{
	LLVMValueRef llvmFunction = module->functionValues[module->currentFunction];
	LLVMBasicBlockRef beforeBlock = LLVMGetInsertBlock(module->builder);
	LLVMBasicBlockRef headerBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "while.header");
	LLVMBasicBlockRef loopBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "while.loop");
	LLVMBasicBlockRef mergeBlock = LLVMCreateBasicBlockInContext(llb->llvmContext, "while.merge");

	statement->breakHandle = mergeBlock;
	statement->continueHandle = headerBlock;

	LLVM_CALL(LLVMBuildBr, module->builder, headerBlock);
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, headerBlock);

	LLVMValueRef condition = GenExpression(llb, module, statement->condition);
	condition = GetRValue(llb, module, condition, statement->condition->lvalue);
	condition = LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntNE, condition, LLVMConstNull(LLVMTypeOf(condition)), "");
	LLVM_CALL(LLVMBuildCondBr, module->builder, condition, loopBlock, mergeBlock);

	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, loopBlock);
	GenStatement(llb, module, statement->body);
	if (!BlockHasBranched(llb, module)) {
		LLVM_CALL(LLVMBuildBr, module->builder, headerBlock);
	}

	LLVM_CALL(LLVMAppendExistingBasicBlock, llvmFunction, mergeBlock);
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, mergeBlock);
}

static void GenForLoop(LLVMBackend* llb, SkModule* module, AstForLoop* statement)
{
	LLVMValueRef llvmFunction = module->functionValues[module->currentFunction];
	LLVMBasicBlockRef beforeBlock = LLVMGetInsertBlock(module->builder);
	LLVMBasicBlockRef headerBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "for.header");
	LLVMBasicBlockRef loopBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "for.loop");
	LLVMBasicBlockRef iterateBlock = LLVMCreateBasicBlockInContext(llb->llvmContext, "for.iterate");
	LLVMBasicBlockRef mergeBlock = LLVMCreateBasicBlockInContext(llb->llvmContext, "for.merge");

	statement->breakHandle = mergeBlock;
	statement->continueHandle = headerBlock;

	LLVMValueRef start = NULL;
	LLVMValueRef end = NULL;
	LLVMValueRef delta = NULL;

	start = GenExpression(llb, module, statement->startValue);
	start = GetRValue(llb, module, start, statement->startValue->lvalue);
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(start)) == LLVMIntegerTypeKind);
	start = CastInt(llb, module, start, LLVMInt32TypeInContext(llb->llvmContext), statement->startValue->type);

	LLVMValueRef it = AllocateLocalVariable(llb, module, LLVMInt32TypeInContext(llb->llvmContext), statement->iteratorName);
	if (module->hasDebugInfo)
	{
		DebugInfoDeclareVariable(llb, module, it, GetIntegerType(32, true), statement->iteratorName, statement->inputState.line, statement->inputState.col);
	}
	LLVM_CALL(LLVMBuildStore, module->builder, start, it);
	statement->iterator->allocHandle = it;

	LLVM_CALL(LLVMBuildBr, module->builder, headerBlock);
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, headerBlock);

	end = GenExpression(llb, module, statement->endValue);
	end = GetRValue(llb, module, end, statement->endValue->lvalue);
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(end)) == LLVMIntegerTypeKind);
	end = CastValue(llb, module, end, LLVMInt32TypeInContext(llb->llvmContext), statement->endValue->type, GetIntegerType(32, false));

	if (statement->deltaValue) {
		delta = GenExpression(llb, module, statement->endValue);
		delta = GetRValue(llb, module, delta, statement->endValue->lvalue);
		SnekAssert(LLVMGetTypeKind(LLVMTypeOf(delta)) == LLVMIntegerTypeKind && LLVMIsConstant(delta));
	}
	else
	{
		delta = LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 1, true);
	}

	LLVMValueRef itValue = LLVM_CALL(LLVMBuildLoad, module->builder, it, "");
	SnekAssert(statement->direction != 0);
	//LLVMIntPredicate comparisonType = statement->direction > 0 ? LLVMIntSLE : LLVMIntSGE;
	LLVMValueRef condition = LLVM_CALL(LLVMBuildICmp, module->builder, LLVMIntNE, itValue, end, "");
	LLVM_CALL(LLVMBuildCondBr, module->builder, condition, loopBlock, mergeBlock);

	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, loopBlock);
	GenStatement(llb, module, statement->body);
	if (!BlockHasBranched(llb, module)) {
		LLVM_CALL(LLVMBuildBr, module->builder, iterateBlock);
	}

	LLVM_CALL(LLVMAppendExistingBasicBlock, llvmFunction, iterateBlock);
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, iterateBlock);

	LLVMValueRef nextItValue = LLVM_CALL(LLVMBuildAdd, module->builder, itValue, delta, "");
	LLVM_CALL(LLVMBuildStore, module->builder, nextItValue, it);
	LLVM_CALL(LLVMBuildBr, module->builder, headerBlock);

	LLVM_CALL(LLVMAppendExistingBasicBlock, llvmFunction, mergeBlock);
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, mergeBlock);
}

static void GenBreak(LLVMBackend* llb, SkModule* module, AstBreak* statement)
{
	if (statement->branchDst->statementKind == STATEMENT_KIND_WHILE)
	{
		AstWhileLoop* whileLoop = (AstWhileLoop*)statement->branchDst;
		LLVMBasicBlockRef mergeBlock = (LLVMBasicBlockRef)whileLoop->breakHandle;
		LLVM_CALL(LLVMBuildBr, module->builder, mergeBlock);
	}
	else if (statement->branchDst->statementKind == STATEMENT_KIND_FOR)
	{
		AstForLoop* forLoop = (AstForLoop*)statement->branchDst;
		LLVMBasicBlockRef mergeBlock = (LLVMBasicBlockRef)forLoop->breakHandle;
		LLVM_CALL(LLVMBuildBr, module->builder, mergeBlock);
	}
	else
	{
		SnekAssert(false);
	}
}

static void GenContinue(LLVMBackend* llb, SkModule* module, AstContinue* statement)
{
	if (statement->branchDst->statementKind == STATEMENT_KIND_WHILE)
	{
		AstWhileLoop* whileLoop = (AstWhileLoop*)statement->branchDst;
		LLVMBasicBlockRef headerBlock = (LLVMBasicBlockRef)whileLoop->continueHandle;
		LLVM_CALL(LLVMBuildBr, module->builder, headerBlock);
	}
	else if (statement->branchDst->statementKind == STATEMENT_KIND_FOR)
	{
		AstForLoop* forLoop = (AstForLoop*)statement->branchDst;
		LLVMBasicBlockRef headerBlock = (LLVMBasicBlockRef)forLoop->continueHandle;
		LLVM_CALL(LLVMBuildBr, module->builder, headerBlock);
	}
	else
	{
		SnekAssert(false);
	}
}

static void GenReturn(LLVMBackend* llb, SkModule* module, AstReturn* statement)
{
	if (statement->value)
	{
		AstFunction* function = module->currentFunction;
		LLVMTypeRef returnType = LLVMGetReturnType(LLVMGetElementType(LLVMTypeOf((LLVMValueRef)function->valueHandle)));

		LLVMValueRef value = GenExpression(llb, module, statement->value);
		value = GetRValue(llb, module, value, statement->value->lvalue);

		value = CastValue(llb, module, value, returnType, statement->value->type, function->returnType->typeID);

		LLVM_CALL(LLVMBuildStore, module->builder, value, module->returnAlloc);
	}

	LLVM_CALL(LLVMBuildBr, module->builder, module->returnBlock);
}

static void GenFree(LLVMBackend* llb, SkModule* module, AstFree* statement)
{
	for (int i = 0; i < statement->values.size; i++)
	{
		LLVMValueRef value = GenExpression(llb, module, statement->values[i]);
		value = GetRValue(llb, module, value, statement->values[i]->lvalue);

		if (statement->values[i]->type->typeKind == TYPE_KIND_POINTER)
		{
			LLVMBuildFree(module->builder, value);
		}
		else if (statement->values[i]->type->typeKind == TYPE_KIND_CLASS)
		{
			// TODO call destructor
			LLVMBuildFree(module->builder, value);
		}
	}
}

static void GenStatement(LLVMBackend* llb, SkModule* module, AstStatement* statement)
{
	if (module->hasDebugInfo)
	{
		DebugInfoEmitSourceLocation(llb, module, statement);
	}

	switch (statement->statementKind)
	{
	case STATEMENT_KIND_NO_OP:
		break;
	case STATEMENT_KIND_COMPOUND:
		GenCompoundStatement(llb, module, (AstCompoundStatement*)statement);
		break;
	case STATEMENT_KIND_VAR_DECL:
		GenVarDeclStatement(llb, module, (AstVarDeclStatement*)statement);
		break;
	case STATEMENT_KIND_EXPR:
		GenExprStatement(llb, module, (AstExprStatement*)statement);
		break;
	case STATEMENT_KIND_IF:
		GenIfStatement(llb, module, (AstIfStatement*)statement);
		break;
	case STATEMENT_KIND_WHILE:
		GenWhileLoop(llb, module, (AstWhileLoop*)statement);
		break;
	case STATEMENT_KIND_FOR:
		GenForLoop(llb, module, (AstForLoop*)statement);
		break;
	case STATEMENT_KIND_BREAK:
		GenBreak(llb, module, (AstBreak*)statement);
		break;
	case STATEMENT_KIND_CONTINUE:
		GenContinue(llb, module, (AstContinue*)statement);
		break;
	case STATEMENT_KIND_RETURN:
		GenReturn(llb, module, (AstReturn*)statement);
		break;
	case STATEMENT_KIND_FREE:
		GenFree(llb, module, (AstFree*)statement);
		break;

	default:
		SnekAssert(false);
		break;
	}
}

static LLVMValueRef GenFunctionHeader(LLVMBackend* llb, SkModule* module, AstFunction* decl)
{
	AstFunction* lastFunction = module->currentFunction;
	module->currentFunction = decl;

	LLVMLinkage linkage = LLVMInternalLinkage;
	if (decl->flags & DECL_FLAG_EXTERN || !decl->body)
		linkage = LLVMExternalLinkage;
	else if (decl->flags & DECL_FLAG_LINKAGE_DLLEXPORT)
		linkage = LLVMDLLExportLinkage;
	else if (decl->flags & DECL_FLAG_LINKAGE_DLLIMPORT)
		linkage = LLVMDLLImportLinkage;
	else if (decl->visibility == VISIBILITY_PRIVATE)
		linkage = LLVMPrivateLinkage;
	else if (decl->visibility == VISIBILITY_PUBLIC)
		linkage = LLVMExternalLinkage;


	LLVMTypeRef functionReturnType = NULL;
	List<LLVMTypeRef> functionParamTypes = CreateList<LLVMTypeRef>();

	LLVMTypeRef returnType = GenType(llb, module, decl->returnType);
	if (LLVMTypeRef _returnType = CanPassByValue(llb, module, returnType))
	{
		functionReturnType = _returnType;
	}
	else
	{
		functionReturnType = LLVMVoidTypeInContext(llb->llvmContext);
		functionParamTypes.add(LLVMPointerType(returnType, 0));
	}

	int numParams = decl->paramTypes.size;
	for (int i = 0; i < numParams; i++)
	{
		LLVMTypeRef paramType = GenType(llb, module, decl->paramTypes[i]);
		if (LLVMTypeRef _paramType = CanPassByValue(llb, module, paramType))
		{
			functionParamTypes.add(_paramType);
		}
		else
		{
			functionParamTypes.add(LLVMPointerType(paramType, 0));
		}
	}

	LLVMValueRef llvmValue = CreateFunction(decl->mangledName, functionReturnType, functionParamTypes.size, functionParamTypes.buffer, decl->varArgs, module->llvmModule, linkage);
	decl->valueHandle = llvmValue;

	module->functionValues.emplace(decl, llvmValue);

	module->currentFunction = lastFunction;

	return llvmValue;
}

static LLVMValueRef GenFunction(LLVMBackend* llb, SkModule* module, AstFunction* decl)
{
	AstFunction* lastFunction = module->currentFunction;
	module->currentFunction = decl;

	LLVMValueRef llvmValue = NULL;
	if (module->functionValues.find(decl) != module->functionValues.end())
		llvmValue = module->functionValues[decl];
	else
	{
		//llvmValue = GenFunctionHeader(llb, module, decl);
		SnekAssert(false);
	}

	if (decl->body)
	{
		LLVMTypeRef returnType = GenType(llb, module, decl->returnType);
		bool returnValueAsArg = !CanPassByValue(llb, module, returnType);

		LLVMBasicBlockRef parentBlock = LLVM_CALL(LLVMGetInsertBlock, module->builder);
		LLVMBasicBlockRef entryBlock = LLVM_CALL(LLVMAppendBasicBlockInContext, llb->llvmContext, llvmValue, "entry");
		LLVMBasicBlockRef returnBlock = LLVM_CALL(LLVMCreateBasicBlockInContext, llb->llvmContext, "return");
		LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, entryBlock);

		if (module->hasDebugInfo)
		{
			LLVMMetadataRef subProgram = DebugInfoBeginFunction(llb, module, decl, llvmValue, NULL);
			DebugInfoEmitNullLocation(llb, module, module->builder);
			DebugInfoEmitNullLocation(llb, module, module->entryBuilder);
		}

		module->returnBlock = returnBlock;
		module->returnAlloc = NULL;
		if (decl->returnType->typeKind != TYPE_KIND_VOID)
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

		for (int i = 0; i < decl->paramTypes.size; i++) {
			LLVMValueRef arg = NULL;
			if (!returnValueAsArg)
				arg = LLVM_CALL(LLVMGetParam, llvmValue, i);
			else
				arg = LLVM_CALL(LLVMGetParam, llvmValue, i + 1);

			LLVMTypeRef paramType = GenType(llb, module, decl->paramTypes[i]);
			LLVMValueRef argAlloc = NULL;

			if (LLVMTypeRef _paramType = CanPassByValue(llb, module, paramType))
			{
				argAlloc = AllocateLocalVariable(llb, module, paramType, decl->paramNames[i]);
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
				DebugInfoDeclareParameter(llb, module, argAlloc, i, decl->paramTypes[i]->typeID, decl->paramNames[i], decl->paramTypes[i]->inputState.line, decl->paramTypes[i]->inputState.col);
			}

			decl->paramVariables[i]->allocHandle = argAlloc;
		}

		GenStatement(llb, module, decl->body);

		LLVM_CALL(LLVMAppendExistingBasicBlock, llvmValue, returnBlock);

		if (!BlockHasBranched(llb, module))
			LLVM_CALL(LLVMBuildBr, module->builder, returnBlock);
		if (LLVM_CALL(LLVMGetInsertBlock, module->builder) != returnBlock)
			LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, returnBlock);

		// Emit return location
		if (module->hasDebugInfo)
		{
			DebugInfoEmitSourceLocation(llb, module, module->builder, decl->endInputState.line, decl->endInputState.col);
		}

		if (decl->returnType->typeKind != TYPE_KIND_VOID)
		{
			if (!returnValueAsArg)
				LLVM_CALL(LLVMBuildRet, module->builder, LLVM_CALL(LLVMBuildLoad, module->builder, module->returnAlloc, ""));
			else
				LLVM_CALL(LLVMBuildRetVoid, module->builder);
		}
		else
		{
			LLVM_CALL(LLVMBuildRetVoid, module->builder);
		}

		LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, parentBlock);

		if (module->hasDebugInfo)
		{
			DebugInfoEndFunction(llb, module, decl);
		}
	}

	module->currentFunction = lastFunction;

	return llvmValue;
}

static LLVMTypeRef GenEnumHeader(LLVMBackend* llb, SkModule* module, AstEnum* decl)
{
	return LLVMInt32TypeInContext(llb->llvmContext);
}

static LLVMTypeRef GenEnum(LLVMBackend* llb, SkModule* module, AstEnum* decl)
{
	LLVMValueRef lastValue = NULL;
	for (int i = 0; i < decl->values.size; i++)
	{
		LLVMValueRef entryValue = NULL;
		if (decl->values[i].value)
		{
			entryValue = GenExpression(llb, module, decl->values[i].value);
			entryValue = CastInt(llb, module, entryValue, LLVMInt32TypeInContext(llb->llvmContext), decl->values[i].value->type);
			lastValue = entryValue;
		}
		else
		{
			if (lastValue)
				entryValue = LLVMConstAdd(lastValue, LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 1, false));
			else
				entryValue = LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 0, false);
			lastValue = entryValue;
		}
		decl->values[i].valueHandle = entryValue;
	}

	return LLVMInt32TypeInContext(llb->llvmContext);
}

static LLVMTypeRef GenStructHeader(LLVMBackend* llb, SkModule* module, AstStruct* decl)
{
	LLVMTypeRef llvmType = LLVM_CALL(LLVMStructCreateNamed, llb->llvmContext, decl->mangledName);
	decl->typeHandle = llvmType;

	llb->structTypes.emplace(decl, llvmType);

	return llvmType;
}

static LLVMTypeRef GenStruct(LLVMBackend* llb, SkModule* module, AstStruct* decl)
{
	LLVMTypeRef llvmType = NULL;
	if (llb->structTypes.find(decl) != llb->structTypes.end())
		llvmType = llb->structTypes[decl];
	else
	{
		//llvmType = GenStructHeader(llb, module, decl);
		SnekAssert(false);
	}

	if (decl->hasBody)
	{
		if (decl->fields.size > 0)
		{
			constexpr int MAX_STRUCT_FIELDS = 64;
			static LLVMTypeRef fieldTypes[MAX_STRUCT_FIELDS];

			int numFields = decl->fields.size;
			for (int i = 0; i < numFields; i++)
			{
				fieldTypes[i] = GenType(llb, module, decl->fields[i].type);
			}

			bool isPacked = decl->flags & DECL_FLAG_PACKED;

			LLVM_CALL(LLVMStructSetBody, llvmType, fieldTypes, numFields, isPacked);
		}
		else
		{
			LLVM_CALL(LLVMStructSetBody, llvmType, NULL, 0, false);
		}
	}

	return llvmType;
}

static LLVMTypeRef GenClassHeader(LLVMBackend* llb, SkModule* module, AstClass* decl)
{
	LLVMTypeRef llvmType = LLVM_CALL(LLVMStructCreateNamed, llb->llvmContext, decl->mangledName);
	decl->typeHandle = llvmType;

	llb->classTypes.emplace(decl, llvmType);

	return llvmType;
}

static LLVMValueRef GenClassMethodHeader(LLVMBackend* llb, SkModule* module, AstFunction* method, AstClass* classDecl)
{
	AstFunction* lastFunction = module->currentFunction;
	module->currentFunction = method;

	LLVMTypeRef returnType = GenType(llb, module, method->returnType);
	int numParams = method->paramTypes.size + 1;
	LLVMTypeRef* paramTypes = new LLVMTypeRef[numParams];
	paramTypes[0] = GenTypeID(llb, module, method->instanceType);
	for (int i = 1; i < numParams; i++)
	{
		paramTypes[i] = GenType(llb, module, method->paramTypes[i - 1]);
	}

	LLVMLinkage linkage = LLVMExternalLinkage;
	LLVMValueRef llvmValue = CreateFunction(method->mangledName, returnType, numParams, paramTypes, method->varArgs, module->llvmModule, linkage);

	module->functionValues.emplace(method, llvmValue);

	method->valueHandle = llvmValue;

	module->currentFunction = lastFunction;

	return llvmValue;
}

static LLVMValueRef GenClassConstructorHeader(LLVMBackend* llb, SkModule* module, AstFunction* constructor, AstClass* classDecl)
{
	AstFunction* lastFunction = module->currentFunction;
	module->currentFunction = constructor;

	LLVMTypeRef returnType = GenTypeID(llb, module, constructor->instanceType);
	int numParams = constructor->paramTypes.size + 1;
	LLVMTypeRef* paramTypes = new LLVMTypeRef[numParams];
	paramTypes[0] = GenTypeID(llb, module, constructor->instanceType);
	for (int i = 1; i < numParams; i++)
	{
		paramTypes[i] = GenType(llb, module, constructor->paramTypes[i - 1]);
	}

	LLVMLinkage linkage = LLVMExternalLinkage;
	LLVMValueRef llvmValue = CreateFunction(constructor->mangledName, returnType, numParams, paramTypes, constructor->varArgs, module->llvmModule, linkage);

	module->functionValues.emplace(constructor, llvmValue);

	constructor->valueHandle = llvmValue;

	module->currentFunction = lastFunction;

	return llvmValue;
}

static void GenClassProcedureHeaders(LLVMBackend* llb, SkModule* module, AstClass* decl)
{
	for (int i = 0; i < decl->methods.size; i++)
	{
		GenClassMethodHeader(llb, module, decl->methods[i], decl);
	}
	if (decl->constructor)
		GenClassConstructorHeader(llb, module, decl->constructor, decl);
}

static LLVMTypeRef GenClass(LLVMBackend* llb, SkModule* module, AstClass* decl)
{
	LLVMTypeRef llvmType = NULL;
	if (llb->classTypes.find(decl) != llb->classTypes.end())
		llvmType = llb->classTypes[decl];
	else
	{
		//llvmType = GenStructHeader(llb, module, decl);
		SnekAssert(false);
	}

	if (decl->fields.size > 0)
	{
		int numFields = decl->fields.size;
		LLVMTypeRef* fieldTypes = new LLVMTypeRef[numFields];
		for (int i = 0; i < numFields; i++)
		{
			fieldTypes[i] = GenType(llb, module, decl->fields[i].type);
		}

		LLVM_CALL(LLVMStructSetBody, llvmType, fieldTypes, numFields, false);
	}

	return llvmType;
}

static LLVMValueRef GenClassMethod(LLVMBackend* llb, SkModule* module, AstFunction* method, AstClass* classDecl)
{
	AstFunction* lastFunction = module->currentFunction;
	module->currentFunction = method;

	LLVMValueRef llvmValue = NULL;
	if (module->functionValues.find(method) != module->functionValues.end())
		llvmValue = module->functionValues[method];
	else
	{
		//llvmValue = GenFunctionHeader(llb, module, decl);
		SnekAssert(false);
	}

	SnekAssert(method->body != NULL);

	if (module->hasDebugInfo)
	{
		DebugInfoBeginFunction(llb, module, method, llvmValue, method->instanceType);
		DebugInfoEmitNullLocation(llb, module, module->builder);
	}

	LLVMTypeRef returnType = LLVM_CALL(LLVMGetReturnType, LLVMGetElementType(LLVMTypeOf(llvmValue)));

	LLVMBasicBlockRef parentBlock = LLVM_CALL(LLVMGetInsertBlock, module->builder);
	LLVMBasicBlockRef entryBlock = LLVM_CALL(LLVMAppendBasicBlockInContext, llb->llvmContext, llvmValue, "entry");
	LLVMBasicBlockRef returnBlock = LLVM_CALL(LLVMCreateBasicBlockInContext, llb->llvmContext, "return");
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, entryBlock);

	LLVMValueRef instance = LLVM_CALL(LLVMGetParam, llvmValue, 0);
	LLVMValueRef instanceAlloc = AllocateLocalVariable(llb, module, GenTypeID(llb, module, method->instanceType), "this");
	LLVM_CALL(LLVMBuildStore, module->builder, instance, instanceAlloc);
	method->instanceVariable->allocHandle = instanceAlloc;

	for (int i = 0; i < method->paramTypes.size; i++) {
		LLVMValueRef arg = LLVM_CALL(LLVMGetParam, llvmValue, (int)i + 1);
		LLVMTypeRef paramType = GenType(llb, module, method->paramTypes[i]);
		LLVMValueRef argAlloc = AllocateLocalVariable(llb, module, paramType, method->paramNames[i]);
		LLVM_CALL(LLVMBuildStore, module->builder, arg, argAlloc);

		method->paramVariables[i]->allocHandle = argAlloc;
	}

	module->returnBlock = returnBlock;
	module->returnAlloc = method->returnType->typeKind != TYPE_KIND_VOID ? AllocateLocalVariable(llb, module, returnType, "__return") : NULL;

	GenStatement(llb, module, method->body);

	if (module->hasDebugInfo)
	{
		DebugInfoEndFunction(llb, module, method);
	}

	LLVM_CALL(LLVMAppendExistingBasicBlock, llvmValue, returnBlock);

	if (!BlockHasBranched(llb, module))
		LLVM_CALL(LLVMBuildBr, module->builder, returnBlock);
	if (LLVM_CALL(LLVMGetInsertBlock, module->builder) != returnBlock)
		LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, returnBlock);

	if (module->returnAlloc) {
		LLVM_CALL(LLVMBuildRet, module->builder, LLVM_CALL(LLVMBuildLoad, module->builder, module->returnAlloc, ""));
	}
	else {
		LLVM_CALL(LLVMBuildRetVoid, module->builder);
	}


	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, parentBlock);

	module->currentFunction = lastFunction;

	return llvmValue;
}

static LLVMValueRef GenClassConstructor(LLVMBackend* llb, SkModule* module, AstFunction* constructor, AstClass* classDecl)
{
	AstFunction* lastFunction = module->currentFunction;
	module->currentFunction = constructor;

	LLVMValueRef llvmValue = NULL;
	if (module->functionValues.find(constructor) != module->functionValues.end())
		llvmValue = module->functionValues[constructor];
	else
	{
		//llvmValue = GenFunctionHeader(llb, module, decl);
		SnekAssert(false);
	}

	SnekAssert(constructor->body != NULL);

	if (module->hasDebugInfo)
	{
		DebugInfoBeginFunction(llb, module, constructor, llvmValue, constructor->instanceType);
		DebugInfoEmitNullLocation(llb, module, module->builder);
	}

	LLVMTypeRef returnType = LLVM_CALL(LLVMGetReturnType, LLVMGetElementType(LLVMTypeOf(llvmValue)));

	LLVMBasicBlockRef parentBlock = LLVM_CALL(LLVMGetInsertBlock, module->builder);
	LLVMBasicBlockRef entryBlock = LLVM_CALL(LLVMAppendBasicBlockInContext, llb->llvmContext, llvmValue, "entry");
	LLVMBasicBlockRef returnBlock = LLVM_CALL(LLVMCreateBasicBlockInContext, llb->llvmContext, "return");
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, entryBlock);

	LLVMValueRef instance = LLVM_CALL(LLVMGetParam, llvmValue, 0);
	LLVMValueRef instanceAlloc = AllocateLocalVariable(llb, module, GenTypeID(llb, module, constructor->instanceType), "this");
	LLVM_CALL(LLVMBuildStore, module->builder, instance, instanceAlloc);
	constructor->instanceVariable->allocHandle = instanceAlloc;

	for (int i = 0; i < constructor->paramTypes.size; i++) {
		LLVMValueRef arg = LLVM_CALL(LLVMGetParam, llvmValue, (int)i + 1);
		LLVMTypeRef paramType = GenType(llb, module, constructor->paramTypes[i]);
		LLVMValueRef argAlloc = AllocateLocalVariable(llb, module, paramType, constructor->paramNames[i]);
		LLVM_CALL(LLVMBuildStore, module->builder, arg, argAlloc);

		constructor->paramVariables[i]->allocHandle = argAlloc;
	}

	module->returnBlock = returnBlock;

	GenStatement(llb, module, constructor->body);

	if (module->hasDebugInfo)
	{
		DebugInfoEndFunction(llb, module, constructor);
	}

	LLVM_CALL(LLVMAppendExistingBasicBlock, llvmValue, returnBlock);

	if (!BlockHasBranched(llb, module))
		LLVM_CALL(LLVMBuildBr, module->builder, returnBlock);
	if (LLVM_CALL(LLVMGetInsertBlock, module->builder) != returnBlock)
		LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, returnBlock);

	LLVM_CALL(LLVMBuildRet, module->builder, instance);

	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, parentBlock);

	module->currentFunction = lastFunction;

	return llvmValue;
}

static void GenClassProcedures(LLVMBackend* llb, SkModule* module, AstClass* decl)
{
	for (int i = 0; i < decl->methods.size; i++)
	{
		GenClassMethod(llb, module, decl->methods[i], decl);
	}
	if (decl->constructor)
		GenClassConstructor(llb, module, decl->constructor, decl);
}

static LLVMValueRef GenGlobalHeader(LLVMBackend* llb, SkModule* module, AstGlobal* decl)
{
	LLVMTypeRef llvmType = GenType(llb, module, decl->type);
	LLVMValueRef alloc = LLVMAddGlobal(module->llvmModule, llvmType, decl->name);

	bool isConstant = decl->flags & DECL_FLAG_CONSTANT;
	if (isConstant)
	{
		LLVM_CALL(LLVMSetGlobalConstant, alloc, true);
		LLVM_CALL(LLVMSetLinkage, alloc, LLVMPrivateLinkage);
	}
	else
	{
		LLVMLinkage linkage = LLVMInternalLinkage;
		if (decl->flags & DECL_FLAG_EXTERN)
			linkage = LLVMExternalLinkage;
		else if (decl->flags & DECL_FLAG_LINKAGE_DLLEXPORT)
			linkage = LLVMDLLExportLinkage;
		else if (decl->flags & DECL_FLAG_LINKAGE_DLLIMPORT)
			linkage = LLVMDLLImportLinkage;
		else if (decl->variable->visibility == VISIBILITY_PRIVATE)
			linkage = LLVMPrivateLinkage;
		else if (decl->variable->visibility == VISIBILITY_PUBLIC)
			linkage = LLVMExternalLinkage;

		LLVM_CALL(LLVMSetLinkage, alloc, linkage);
	}

	decl->variable->allocHandle = alloc;
	module->globalValues.emplace(decl, alloc);

	return alloc;
}

static void GenGlobal(LLVMBackend* llb, SkModule* module, AstGlobal* global)
{
	LLVMValueRef alloc = NULL;
	if (module->globalValues.find(global) != module->globalValues.end())
		alloc = module->globalValues[global];
	else
	{
		//llvmValue = GenGlobalHeader(llb, module, global);
		SnekAssert(false);
	}

	LLVM_CALL(LLVMSetExternallyInitialized, alloc, false);

	LLVMTypeRef llvmType = LLVMGetElementType(LLVMTypeOf(alloc));

	bool isExtern = global->flags & DECL_FLAG_EXTERN;

	if (global->value)
	{
		LLVMValueRef value = GenExpression(llb, module, global->value);
		value = GetRValue(llb, module, value, global->value->lvalue);
		value = ConstCastValue(llb, module, value, llvmType, global->value->type, global->type->typeID);

		/*
		if (global->value->exprKind == EXPR_KIND_STRING_LITERAL)
		{
			LLVMTypeRef stringType = GetStringType(llb);
			value = LLVM_CALL(LLVMConstBitCast, value, stringType);
		}
		*/

		LLVM_CALL(LLVMSetInitializer, alloc, value);
	}
	else if (!isExtern)
	{
		LLVM_CALL(LLVMSetInitializer, alloc, LLVMConstNull(llvmType));
	}

	if (module->hasDebugInfo)
	{
		DebugInfoDeclareGlobal(llb, module, alloc, global->name, global->name, global->type->typeID, global->inputState.line);
	}
}

static void GenImport(LLVMBackend* llb, SkModule* module, AstImport* decl)
{
}

static void GenBuiltInDecls(LLVMBackend* llb, SkModule* module)
{
}

static SkModule** GenModules(LLVMBackend* llb, int numModules, AstFile** asts, bool genDebugInfo)
{
	SkModule** modules = new SkModule * [numModules];

	for (int i = 0; i < numModules; i++)
	{
		AstFile* ast = asts[i];
		SkModule* module = CreateModule(ast, genDebugInfo);
		modules[i] = module;

		LLVMModuleRef llvmModule = LLVMModuleCreateWithNameInContext(module->ast->name, llb->llvmContext);
		LLVMBuilderRef builder = LLVMCreateBuilderInContext(llb->llvmContext);
		LLVMBuilderRef entryBuilder = LLVMCreateBuilderInContext(llb->llvmContext);
		LLVMDIBuilderRef diBuilder = LLVMCreateDIBuilder(llvmModule);

		LLVMSetModuleDataLayout(llvmModule, llb->targetData);
		LLVMSetTarget(llvmModule, llb->targetTriple);

		module->llvmModule = llvmModule;
		module->builder = builder;
		module->entryBuilder = entryBuilder;
		module->diBuilder = diBuilder;
	}


	// Generate imports

	if (genDebugInfo)
	{
		for (int i = 0; i < numModules; i++)
		{
			SkModule* module = modules[i];
			InitDebugInfo(llb, module, module->ast->sourceFile->filename, module->ast->sourceFile->directory);
		}
	}
	for (int i = 0; i < numModules; i++)
	{
		SkModule* module = modules[i];
		GenBuiltInDecls(llb, module);

		for (int j = 0; j < module->ast->imports.size; j++)
		{
			GenImport(llb, module, module->ast->imports[j]);
		}
	}


	// Generate types

	for (int i = 0; i < numModules; i++)
	{
		SkModule* module = modules[i];
		for (int j = 0; j < module->ast->enums.size; j++)
		{
			GenEnumHeader(llb, module, module->ast->enums[j]);
		}
		for (int j = 0; j < module->ast->structs.size; j++)
		{
			GenStructHeader(llb, module, module->ast->structs[j]);
		}
		for (int j = 0; j < module->ast->classes.size; j++)
		{
			GenClassHeader(llb, module, module->ast->classes[j]);
		}
	}
	for (int i = 0; i < numModules; i++)
	{
		SkModule* module = modules[i];
		for (int j = 0; j < module->ast->enums.size; j++)
		{
			GenEnum(llb, module, module->ast->enums[j]);
		}
		for (int j = 0; j < module->ast->structs.size; j++)
		{
			GenStruct(llb, module, module->ast->structs[j]);
		}
		for (int j = 0; j < module->ast->classes.size; j++)
		{
			GenClass(llb, module, module->ast->classes[j]);
		}
	}


	// Generate variables and functions

	for (int i = 0; i < numModules; i++)
	{
		SkModule* module = modules[i];
		for (int j = 0; j < module->ast->globals.size; j++)
		{
			GenGlobalHeader(llb, module, module->ast->globals[j]);
		}
		for (int j = 0; j < module->ast->functions.size; j++)
		{
			GenFunctionHeader(llb, module, module->ast->functions[j]);
		}
		for (int j = 0; j < module->ast->classes.size; j++)
		{
			GenClassProcedureHeaders(llb, module, module->ast->classes[j]);
		}
	}
	for (int i = 0; i < numModules; i++)
	{
		SkModule* module = modules[i];
		for (int j = 0; j < module->ast->globals.size; j++)
		{
			GenGlobal(llb, module, module->ast->globals[j]);
		}
	}
	for (int i = 0; i < numModules; i++)
	{
		SkModule* module = modules[i];
		for (int j = 0; j < module->ast->functions.size; j++)
		{
			GenFunction(llb, module, module->ast->functions[j]);
		}
		for (int j = 0; j < module->ast->classes.size; j++)
		{
			GenClassProcedures(llb, module, module->ast->classes[j]);
		}
	}

	if (genDebugInfo)
	{
		for (int i = 0; i < numModules; i++)
		{
			SkModule* module = modules[i];
			CompleteDebugInfo(llb, module);
		}
	}

	return modules;
}

static void RetrieveModuleString(AstModule* module, AstFile* file, std::stringstream& stream)
{
	if (module->parent)
	{
		RetrieveModuleString(module->parent, file, stream);
		if (!(strcmp(file->name, module->name) == 0 && file->module == module))
			stream << module->name << '.';
	}
}

static void OutputModule(LLVMBackend* llb, AstFile* ast, LLVMModuleRef llvmModule, const char* buildFolder, bool emitLLVM, List<LinkerFile>& outFiles)
{
	if (emitLLVM)
	{
		std::stringstream filename;
		filename << buildFolder << '/';
		RetrieveModuleString(ast->module, ast, filename);
		filename << ast->name << ".ll";

		std::string filenameStr = filename.str();

		/*
		const char* extension = ".ll";

		int pathLen = (int)(strlen(buildFolder) + 1 + strlen(ast->name) + strlen(extension));
		char* outputPath = new char[pathLen + 1];
		strcpy(outputPath, buildFolder);
		strcpy(outputPath + strlen(outputPath), "/");
		strcpy(outputPath + strlen(outputPath), ast->name);
		strcpy(outputPath + strlen(outputPath), extension);
		outputPath[pathLen] = 0;
		*/

		char* error = NULL;
		if (LLVMPrintModuleToFile(llvmModule, filenameStr.c_str(), &error))
		{
			SnekFatal(llb->context, ERROR_CODE_BACKEND_ERROR, "Can't output LLVM IR to file '%s'", filenameStr.c_str());
			LLVMDisposeMessage(error);
		}

		//delete outputPath;
	}
	else
	{
		std::stringstream filename;
		filename << buildFolder << '/';
		RetrieveModuleString(ast->module, ast, filename);
		filename << ast->name << ".obj";

		std::string filenameStr = filename.str();

		/*
		const char* extension = ".obj";

		int pathLen = (int)(strlen(buildFolder) + 1 + strlen(ast->name) + strlen(extension));
		char* outputPath = new char[pathLen + 1];
		strcpy(outputPath, buildFolder);
		strcpy(outputPath + strlen(outputPath), "/");
		strcpy(outputPath + strlen(outputPath), ast->name);
		strcpy(outputPath + strlen(outputPath), extension);
		outputPath[pathLen] = 0;
		*/

		LLVMSetModuleDataLayout(llvmModule, LLVMCreateTargetDataLayout(llb->targetMachine));
		LLVMSetTarget(llvmModule, llb->targetTriple);

		LLVMCodeGenFileType fileType = LLVMObjectFile;

		char* error = NULL;
		if (LLVMTargetMachineEmitToFile(llb->targetMachine, llvmModule, (char*)filenameStr.c_str(), fileType, &error))
		{
			SnekFatal(llb->context, 0, "%s", error);
			LLVMDisposeMessage(error);
			return;
		}

		outFiles.add({ _strdup(filenameStr.c_str()) });
	}
}

bool LLVMBackendCompile(LLVMBackend* llb, AstFile** asts, int numModules, const char* filename, const char* buildFolder, bool genDebugInfo, bool emitLLVM, int optLevel)
{
	LLVMInitializeX86TargetInfo();
	LLVMInitializeX86Target();
	LLVMInitializeX86TargetMC();
	LLVMInitializeX86AsmParser();
	LLVMInitializeX86AsmPrinter();

	char* error = NULL;
	LLVMTargetRef target = NULL;
	char* targetTriple = LLVMGetDefaultTargetTriple();
	targetTriple = LLVMNormalizeTargetTriple(targetTriple);
	if (LLVMGetTargetFromTriple(targetTriple, &target, &error))
	{
		SnekFatal(llb->context, 0, "%s", error);
		LLVMDisposeMessage(error);
		return false;
	}

	const char* cpu = "generic";
	const char* features = "";
	LLVMCodeGenOptLevel codegenOptLevel = LLVMCodeGenLevelDefault;
	LLVMRelocMode relocModel = LLVMRelocDefault;
	LLVMCodeModel codeModel = LLVMCodeModelDefault;
	LLVMTargetMachineRef targetMachine = LLVMCreateTargetMachine(target, targetTriple, cpu, features, codegenOptLevel, relocModel, codeModel);
	LLVMTargetDataRef targetData = LLVMCreateTargetDataLayout(targetMachine);
	llb->targetMachine = targetMachine;
	llb->targetData = targetData;
	llb->targetTriple = targetTriple;

	SkModule** modules = GenModules(llb, numModules, asts, genDebugInfo);

	LLVMPassManagerRef optPasses = NULL;
	LLVMPassManagerRef debugPasses = NULL;

	bool verify = false;
#if defined(_DEBUG)
	verify = true;
#endif

	if (verify)
	{
		debugPasses = LLVMCreatePassManager();

		LLVMAddVerifierPass(debugPasses);
	}

	if (optLevel > 0)
	{
		optPasses = LLVMCreatePassManager();

		LLVMAddCFGSimplificationPass(optPasses);
		LLVMAddReassociatePass(optPasses);
		LLVMAddGVNPass(optPasses);
		LLVMAddInstructionCombiningPass(optPasses);
		LLVMAddAggressiveInstCombinerPass(optPasses);
	}

	if (optPasses)
	{
		for (int i = 0; i < numModules; i++)
		{
			LLVMModuleRef llvmModule = modules[i]->llvmModule;
			LLVMRunPassManager(optPasses, llvmModule);
		}
	}

	if (debugPasses)
	{
		for (int i = 0; i < numModules; i++)
		{
			LLVMModuleRef llvmModule = modules[i]->llvmModule;
			LLVMRunPassManager(debugPasses, llvmModule);
		}
	}

	for (int i = 0; i < numModules; i++)
	{
		OutputModule(llb, asts[i], modules[i]->llvmModule, buildFolder, emitLLVM, llb->context->linkerFiles);
	}

	if (optPasses)
		LLVMDisposePassManager(optPasses);
	if (debugPasses)
		LLVMDisposePassManager(debugPasses);

	delete modules;

	return true;
}
