#include "llvm_backend.h"

#include "snek.h"
#include "log.h"
#include "debug.h"
#include "function.h"
#include "types.h"
#include "values.h"
#include "operators.h"
#include "debug_info.h"
#include "generics.h"

#include "ast/File.h"
#include "semantics/Type.h"
#include "semantics/Variable.h"

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

static SkModule* CreateModule(AST::File* ast, bool hasDebugInfo)
{
	SkModule* module = new SkModule;
	module->ast = ast;
	module->hasDebugInfo = hasDebugInfo;

	module->debugScopes = CreateList<LLVMMetadataRef>();

	return module;
}

static LLVMTypeRef GenTypeID(LLVMBackend* llb, SkModule* module, TypeID type, AST::Type* ast = NULL);
static LLVMValueRef GenExpression(LLVMBackend* llb, SkModule* module, AST::Expression* expression);

//static LLVMTypeRef GenStructHeader(LLVMBackend* llb, SkModule* module, AST::Struct* decl);
static LLVMTypeRef GenClassHeader(LLVMBackend* llb, SkModule* module, AST::Class* decl);

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
	case FloatingPointPrecision::Half:
		return LLVM_CALL(LLVMHalfTypeInContext, llb->llvmContext);
	case FloatingPointPrecision::Single:
		return LLVM_CALL(LLVMFloatTypeInContext, llb->llvmContext);
	case FloatingPointPrecision::Double:
		return LLVM_CALL(LLVMDoubleTypeInContext, llb->llvmContext);
	case FloatingPointPrecision::Quad:
		return LLVM_CALL(LLVMFP128TypeInContext, llb->llvmContext);
	default:
		SnekAssert(false);
		return nullptr;
	}
}

static LLVMTypeRef GenBoolType(LLVMBackend* llb, SkModule* module)
{
	return LLVM_CALL(LLVMInt1TypeInContext, llb->llvmContext);
}

static LLVMTypeRef GenStructType(LLVMBackend* llb, SkModule* module, TypeID type, AST::NamedType* ast)
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

	if (ast && ast->declaration && ast->declaration->type == AST::DeclarationType::Struct && ((AST::Struct*)ast->declaration)->isGenericInstance)
	{
		AST::Struct* declaration = (AST::Struct*)ast->declaration;

		LLVMTypeRef structType = nullptr;
		if (declaration->typeHandle)
		{
			structType = (LLVMTypeRef)declaration->typeHandle;
		}
		else
		{
			List<LLVMTypeRef> genericTypes;
			for (int i = 0; i < ast->genericArgs.size; i++)
			{
				genericTypes.add(GenType(llb, module, ast->genericArgs[i]));
			}

			structType = GenGenericStructInstance(llb, module, declaration, genericTypes);
		}

		return structType;
	}
	else
	{
		LLVMTypeRef structType = (LLVMTypeRef)type->structType.declaration->typeHandle;

		return structType;
	}
}

static LLVMTypeRef GenClassType(LLVMBackend* llb, SkModule* module, TypeID type, AST::NamedType* ast)
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

static LLVMTypeRef GenAliasType(LLVMBackend* llb, SkModule* module, TypeID type, AST::NamedType* ast)
{
	if (ast)
	{
		if (ast->declaration->type == AST::DeclarationType::Typedef)
			return GenTypeID(llb, module, type->aliasType.alias, ((AST::Typedef*)ast->declaration)->alias);
		else if (ast->declaration->type == AST::DeclarationType::Enumeration)
			return GenTypeID(llb, module, type->aliasType.alias, ((AST::Enum*)ast->declaration)->alias);
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

static LLVMTypeRef GenPointerType(LLVMBackend* llb, SkModule* module, TypeID type, AST::PointerType* ast)
{
	LLVMTypeRef elementType = GenTypeID(llb, module, type->pointerType.elementType, ast ? ast->elementType : NULL);
	if (LLVMGetTypeKind(elementType) == LLVMVoidTypeKind)
		return LLVM_CALL(LLVMPointerType, LLVMInt8TypeInContext(llb->llvmContext), 0);
	else
		return LLVM_CALL(LLVMPointerType, elementType, 0);
}

static LLVMTypeRef GenFunctionType(LLVMBackend* llb, SkModule* module, TypeID type, AST::FunctionType* ast)
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

static LLVMTypeRef GenArrayType(LLVMBackend* llb, SkModule* module, TypeID type, AST::ArrayType* ast)
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

static LLVMTypeRef GenTypeID(LLVMBackend* llb, SkModule* module, TypeID type, AST::Type* ast)
{
	switch (type->typeKind)
	{
	case AST::TypeKind::Void:
		return GenVoidType(llb, module);
	case AST::TypeKind::Integer:
		return GenIntegerType(llb, module, type);
	case AST::TypeKind::FloatingPoint:
		return GenFPType(llb, module, type);
	case AST::TypeKind::Boolean:
		return GenBoolType(llb, module);
	case AST::TypeKind::Struct:
		return GenStructType(llb, module, type, (AST::NamedType*)ast);
	case AST::TypeKind::Class:
		return GenClassType(llb, module, type, (AST::NamedType*)ast);
	case AST::TypeKind::Alias:
		return GenAliasType(llb, module, type, (AST::NamedType*)ast);
	case AST::TypeKind::Pointer:
		return GenPointerType(llb, module, type, (AST::PointerType*)ast);
	case AST::TypeKind::Function:
		return GenFunctionType(llb, module, type, (AST::FunctionType*)ast);

	case AST::TypeKind::Array:
		return GenArrayType(llb, module, type, (AST::ArrayType*)ast);
	case AST::TypeKind::String:
		return GenStringType(llb, module, type);

	default:
		SnekAssert(false);
		return nullptr;
	}
}

LLVMTypeRef GenType(LLVMBackend* llb, SkModule* module, AST::Type* type)
{
	return GenTypeID(llb, module, type->typeID, type);
}

static LLVMValueRef GenExpression(LLVMBackend* llb, SkModule* module, AST::Expression* expression);
//static LLVMValueRef GenFunctionHeader(LLVMBackend* llb, SkModule* module, AST::Function* decl);
static LLVMValueRef GenGlobalHeader(LLVMBackend* llb, SkModule* module, AST::GlobalVariable* global);
static LLVMValueRef GenClassMethodHeader(LLVMBackend* llb, SkModule* module, AST::Function* method, AST::Class* classDecl);

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

static LLVMValueRef GenIntegerLiteral(LLVMBackend* llb, SkModule* module, AST::IntegerLiteral* expression)
{
	LLVMTypeRef type = GenTypeID(llb, module, expression->valueType);
	return LLVM_CALL(LLVMConstInt, type, expression->value, expression->value < 0);
}

static LLVMValueRef GenFPLiteral(LLVMBackend* llb, SkModule* module, AST::FloatingPointLiteral* expression)
{
	return LLVM_CALL(LLVMConstReal, LLVMFloatTypeInContext(llb->llvmContext), expression->value);
}

static LLVMValueRef GenBoolLiteral(LLVMBackend* llb, SkModule* module, AST::BooleanLiteral* expression)
{
	return LLVM_CALL(LLVMConstInt, LLVMInt1TypeInContext(llb->llvmContext), expression->value ? 1 : 0, false);
}

static LLVMValueRef GenCharacterLiteral(LLVMBackend* llb, SkModule* module, AST::CharacterLiteral* expression)
{
	return LLVM_CALL(LLVMConstInt, LLVMInt8TypeInContext(llb->llvmContext), expression->value, false);
}

static LLVMValueRef GenNullLiteral(LLVMBackend* llb, SkModule* module, AST::NullLiteral* expression)
{
	return LLVM_CALL(LLVMConstNull, LLVMPointerType(LLVMInt8TypeInContext(llb->llvmContext), 0));
}

static LLVMValueRef GenStringLiteral(LLVMBackend* llb, SkModule* module, AST::StringLiteral* expression)
{
	return CreateStringLiteral(llb, module, expression->value);
}

static LLVMValueRef GenStructLiteral(LLVMBackend* llb, SkModule* module, AST::StructLiteral* expression)
{
	LLVMTypeRef llvmType = GenType(llb, module, expression->structType);

	if (expression->isConstant())
	{
		int numFields = expression->valueType->structType.numFields;
		LLVMValueRef* values = new LLVMValueRef[numFields];

		for (int i = 0; i < numFields; i++)
		{
			LLVMTypeRef fieldType = GenTypeID(llb, module, expression->structType->typeID->structType.fieldTypes[i]);
			if (i < expression->values.size)
			{
				LLVMValueRef field = GetRValue(llb, module, GenExpression(llb, module, expression->values[i]), expression->values[i]->lvalue);
				if (expression->values[i]->isConstant())
					field = ConstCastValue(llb, module, field, fieldType, expression->values[i]->valueType, expression->structType->typeID->structType.fieldTypes[i]);
				else
					field = CastValue(llb, module, field, fieldType, expression->values[i]->valueType, expression->structType->typeID->structType.fieldTypes[i]);
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
			field = CastValue(llb, module, field, fieldType, expression->values[i]->valueType, expression->structType->typeID->structType.fieldTypes[i]);
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

static LLVMValueRef GenIdentifier(LLVMBackend* llb, SkModule* module, AST::Identifier* expression)
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
		if (expression->file == expression->variable->declaration->file || expression->variable->isConstant)
		{
			value = (LLVMValueRef)expression->variable->allocHandle;
		}
		else
		{
			// Import global
			auto it = module->globalValues.find((AST::GlobalVariable*)expression->variable->declaration);
			if (it != module->globalValues.end())
				value = it->second;
			else
			{
				value = GenGlobalHeader(llb, module, (AST::GlobalVariable*)expression->variable->declaration);
			}
		}
		//}
		return value;
	}
	else if (expression->function)
	{
		if (expression->function->file == expression->file)
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

static LLVMValueRef GenCompoundExpression(LLVMBackend* llb, SkModule* module, AST::CompoundExpression* expression)
{
	return GenExpression(llb, module, expression->value);
}

static LLVMValueRef GenFunctionCall(LLVMBackend* llb, SkModule* module, AST::FunctionCall* expression)
{
	LLVMValueRef callee = NULL;
	TypeID functionType = NULL;

	if (expression->function)
	{
		SnekAssert(!expression->function->isGeneric); // This should be a reference the call's own copy which has isGeneric = false and isGenericInstance = true
		if (expression->function->isGenericInstance)
		{
			List<LLVMTypeRef> genericTypes = CreateList<LLVMTypeRef>();
			for (int i = 0; i < expression->genericArgs.size; i++)
			{
				genericTypes.add(GenType(llb, module, expression->genericArgs[i]));
			}
			callee = GenGenericFunctionInstance(llb, module, expression->function, genericTypes, functionType);

			if (module->hasDebugInfo)
			{
				DebugInfoEmitSourceLocation(llb, module, expression);
			}
		}
		else
		{
			if (expression->function->file == expression->file)
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
						//callee = GenClassMethodHeader(llb, module, expression->function, expression->function->instanceType->classType.declaration);
						callee = GenFunctionHeader(llb, module, expression->function);
					else
						callee = GenFunctionHeader(llb, module, expression->function);
				}
			}
		}

		functionType = expression->function->functionType;
	}
	else if (expression->callee)
	{
		callee = GenExpression(llb, module, expression->callee);
		callee = GetRValue(llb, module, callee, expression->callee->lvalue);
		functionType = expression->callee->valueType;
		while (functionType->typeKind == AST::TypeKind::Alias)
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
		SnekAssert(expression->methodInstance->lvalue || expression->methodInstance->valueType->typeKind == AST::TypeKind::Pointer);
		if (expression->methodInstance->valueType->typeKind == AST::TypeKind::Pointer && expression->methodInstance->lvalue)
			instance = LLVM_CALL(LLVMBuildLoad, module->builder, instance, "");
		//arguments.add(LLVM_CALL(LLVMBuildLoad, module->builder, instance, ""));
		arguments.add(instance);
	}

	int paramCount = numParams - (expression->isMethodCall ? 1 : 0);
	int paramOffset = expression->isMethodCall ? 1 : 0;
	for (int i = 0; i < numArgs; i++)
	{
		LLVMValueRef arg = GenExpression(llb, module, expression->arguments[i]);
		//LLVMTypeRef paramType = paramTypes[i + returnValueAsArg ? 1 : 0];
		if (i < paramCount)
		{
			LLVMTypeRef paramType = GenTypeID(llb, module, functionType->functionType.paramTypes[paramOffset + i]);
			arg = ConvertArgumentValue(llb, module, arg, paramType, expression->arguments[i]->valueType, functionType->functionType.paramTypes[paramOffset + i], expression->arguments[i]->lvalue, expression->arguments[i]->isConstant());

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
			if (expression->arguments[i]->valueType->typeKind == AST::TypeKind::FloatingPoint)
				arg = CastValue(llb, module, arg, LLVMDoubleTypeInContext(llb->llvmContext), expression->arguments[i]->valueType, GetFloatingPointType(FloatingPointPrecision::Double));
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

static LLVMValueRef GenSubscriptOperator(LLVMBackend* llb, SkModule* module, AST::SubscriptOperator* expression)
{
	LLVMValueRef operand = GenExpression(llb, module, expression->operand);

	if (expression->operand->valueType->typeKind == AST::TypeKind::Array ||
		expression->operand->valueType->typeKind == AST::TypeKind::Pointer && expression->operand->valueType->pointerType.elementType->typeKind == AST::TypeKind::Array)
	{
		SnekAssert(expression->lvalue || expression->operand->valueType->typeKind == AST::TypeKind::Pointer);
		SnekAssert(LLVMGetTypeKind(LLVMTypeOf(operand)) == LLVMPointerTypeKind);
		SnekAssert(expression->arguments.size == 1);

		if (expression->lvalue && expression->operand->valueType->typeKind == AST::TypeKind::Pointer)
			operand = LLVMBuildLoad(module->builder, operand, "");

		LLVMValueRef index = GenExpression(llb, module, expression->arguments[0]);
		index = GetRValue(llb, module, index, expression->arguments[0]->lvalue);
		LLVMValueRef element = GetArrayElement(llb, module, operand, index);

		return element;
	}
	else if (expression->operand->valueType->typeKind == AST::TypeKind::String)
	{
		operand = GetRValue(llb, module, operand, expression->operand->lvalue);

		SnekAssert(expression->arguments.size == 1);

		LLVMValueRef index = GenExpression(llb, module, expression->arguments[0]);
		index = GetRValue(llb, module, index, expression->arguments[0]->lvalue);
		LLVMValueRef element = GetStringElement(llb, module, operand, index);

		return element;
	}
	else if (expression->operand->valueType->typeKind == AST::TypeKind::Pointer)
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

static LLVMValueRef GenDotOperator(LLVMBackend* llb, SkModule* module, AST::DotOperator* expression)
{
	LLVMValueRef operand = GenExpression(llb, module, expression->operand);

	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(operand)) == LLVMPointerTypeKind);
	//if (LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(operand))) == LLVMPointerTypeKind)
	//	operand = LLVM_CALL(LLVMBuildLoad, module->builder, operand, "");

	TypeID operandType = expression->operand->valueType;
	if (operandType->typeKind == AST::TypeKind::Pointer)
	{
		operandType = operandType->pointerType.elementType;
		if (expression->operand->lvalue)
		{
			operand = LLVM_CALL(LLVMBuildLoad, module->builder, operand, "");
		}
	}

	while (operandType->typeKind == AST::TypeKind::Alias)
		operandType = operandType->aliasType.alias;

	if (operandType->typeKind == AST::TypeKind::Struct)
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
	else if (operandType->typeKind == AST::TypeKind::Class)
	{
		return GetClassMember(llb, module, operand, expression->classField->index, expression->classField->name);
	}
	else if (operandType->typeKind == AST::TypeKind::Array)
	{
		//operand = GetRValue(llb, module, operand, expression->operand->lvalue);
		SnekAssert(expression->operand->lvalue || expression->operand->valueType->typeKind == AST::TypeKind::Pointer);
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
	else if (operandType->typeKind == AST::TypeKind::String)
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

static LLVMValueRef GenCast(LLVMBackend* llb, SkModule* module, AST::Typecast* expression)
{
	LLVMValueRef value = GenExpression(llb, module, expression->value);
	value = GetRValue(llb, module, value, expression->value->lvalue);
	LLVMTypeRef type = GenType(llb, module, expression->dstType);
	return CastValue(llb, module, value, type, expression->value->valueType, expression->dstType->typeID);
}

static LLVMValueRef GenClassConstructorHeader(LLVMBackend* llb, SkModule* module, AST::Function* constructor, AST::Class* classDecl);

static LLVMValueRef GenSizeof(LLVMBackend* llb, SkModule* module, AST::Sizeof* expression)
{
	LLVMTypeRef llvmType = GenType(llb, module, expression->dstType);
	if (expression->dstType->typeKind == AST::TypeKind::Class)
	{
		llvmType = LLVM_CALL(LLVMGetElementType, llvmType);
	}

	return LLVM_CALL(LLVMSizeOf, llvmType);
}

static LLVMValueRef GenMalloc(LLVMBackend* llb, SkModule* module, AST::Malloc* expression)
{
	LLVMTypeRef llvmType = GenType(llb, module, expression->dstType);
	if (expression->dstType->typeKind == AST::TypeKind::Class || expression->dstType->typeKind == AST::TypeKind::String) // If reference type
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

	if (expression->dstType->typeKind == AST::TypeKind::Class && expression->hasArguments)
	{
		AST::Class* declaration = expression->dstType->typeID->classType.declaration;
		if (AST::Function* constructor = declaration->constructor)
		{
			LLVMValueRef callee = nullptr;
			if (constructor->file == expression->file)
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
				AST::Expression* argExpr = expression->arguments[i - 1];
				TypeID paramTypeID = constructor->paramTypes[i - 1]->typeID;

				LLVMValueRef arg = GenExpression(llb, module, argExpr);
				if (i < numParams)
				{
					LLVMTypeRef paramType = paramTypes[i];
					arg = ConvertArgumentValue(llb, module, arg, paramType, argExpr->valueType, paramTypeID, argExpr->lvalue, LLVMIsConstant(arg));
				}
				else
				{
					arg = GetRValue(llb, module, arg, argExpr->lvalue);
					if (argExpr->valueType->typeKind == AST::TypeKind::FloatingPoint)
						arg = CastValue(llb, module, arg, LLVMDoubleTypeInContext(llb->llvmContext), argExpr->valueType, GetFloatingPointType(FloatingPointPrecision::Double));
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

static LLVMValueRef GenUnaryOperator(LLVMBackend* llb, SkModule* module, AST::UnaryOperator* expression)
{
	LLVMValueRef operand = GenExpression(llb, module, expression->operand);

	switch (expression->operatorType)
	{
	case AST::UnaryOperatorType::Not: return OperatorNot(llb, module, operand, expression->operand->valueType, expression->operand->lvalue);
	case AST::UnaryOperatorType::Negate: return OperatorNegate(llb, module, operand, expression->operand->valueType, expression->operand->lvalue);
	case AST::UnaryOperatorType::Reference: return OperatorReference(llb, module, operand, expression->operand->valueType, expression->operand->lvalue);
	case AST::UnaryOperatorType::Dereference: return OperatorDereference(llb, module, operand, expression->operand->valueType, expression->operand->lvalue);

	case AST::UnaryOperatorType::Increment: return OperatorIncrement(llb, module, operand, expression->operand->valueType, expression->operand->lvalue, expression->position);
	case AST::UnaryOperatorType::Decrement: return OperatorDecrement(llb, module, operand, expression->operand->valueType, expression->operand->lvalue, expression->position);

	default:
		SnekAssert(false);
		return nullptr;
	}
}

static LLVMValueRef GenBinaryOperator(LLVMBackend* llb, SkModule* module, AST::BinaryOperator* expression)
{
	LLVMValueRef left = GenExpression(llb, module, expression->left);
	LLVMValueRef right = GenExpression(llb, module, expression->right);

	switch (expression->operatorType)
	{
	case AST::BinaryOperatorType::Add: return OperatorAdd(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::Subtract: return OperatorSub(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::Multiply: return OperatorMul(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::Divide: return OperatorDiv(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::Modulo: return OperatorMod(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);

	case AST::BinaryOperatorType::Equals: return OperatorEQ(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::DoesNotEqual: return OperatorNE(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::LessThan: return OperatorLT(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::GreaterThan: return OperatorGT(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::LessThanEquals: return OperatorLE(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::GreaterThanEquals: return OperatorGE(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::LogicalAnd: return OperatorAnd(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::LogicalOr: return OperatorOr(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::BitwiseAnd: return OperatorBWAnd(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::BitwiseOr: return OperatorBWOr(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::BitwiseXor: return OperatorBWXor(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::BitshiftLeft: return OperatorBSLeft(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::BitshiftRight: return OperatorBSRight(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);

	case AST::BinaryOperatorType::Assignment: return OperatorAssign(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue, expression->right->isLiteral());
	case AST::BinaryOperatorType::PlusEquals: return OperatorAddAssign(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::MinusEquals: return OperatorSubAssign(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::TimesEquals: return OperatorMulAssign(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::DividedByEquals: return OperatorDivAssign(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::ModuloEquals: return OperatorModAssign(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::BitshiftLeftEquals: return OperatorBSLeftAssign(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::BitshiftRightEquals: return OperatorBSRightAssign(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::BitwiseAndEquals: return OperatorBWAndAssign(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::BitwiseOrEquals: return OperatorBWOrAssign(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);
	case AST::BinaryOperatorType::BitwiseXorEquals: return OperatorBWXorAssign(llb, module, left, right, expression->left->valueType, expression->right->valueType, expression->left->lvalue, expression->right->lvalue);

	default:
		SnekAssert(false);
		return NULL;
	}

	return NULL;
}

static LLVMValueRef GenTernaryOperator(LLVMBackend* llb, SkModule* module, AST::TernaryOperator* expression)
{
	return OperatorTernary(llb, module,
		GenExpression(llb, module, expression->condition),
		GenExpression(llb, module, expression->thenValue),
		GenExpression(llb, module, expression->elseValue),
		expression->condition->valueType,
		expression->thenValue->valueType,
		expression->elseValue->valueType,
		expression->condition->lvalue,
		expression->thenValue->lvalue,
		expression->elseValue->lvalue
	);
}

static LLVMValueRef GenExpression(LLVMBackend* llb, SkModule* module, AST::Expression* expression)
{
	switch (expression->type)
	{
	case AST::ExpressionType::IntegerLiteral:
		return GenIntegerLiteral(llb, module, (AST::IntegerLiteral*)expression);
	case AST::ExpressionType::FloatingPointLiteral:
		return GenFPLiteral(llb, module, (AST::FloatingPointLiteral*)expression);
	case AST::ExpressionType::BooleanLiteral:
		return GenBoolLiteral(llb, module, (AST::BooleanLiteral*)expression);
	case AST::ExpressionType::CharacterLiteral:
		return GenCharacterLiteral(llb, module, (AST::CharacterLiteral*)expression);
	case AST::ExpressionType::NullLiteral:
		return GenNullLiteral(llb, module, (AST::NullLiteral*)expression);
	case AST::ExpressionType::StringLiteral:
		return GenStringLiteral(llb, module, (AST::StringLiteral*)expression);
	case AST::ExpressionType::StructLiteral:
		return GenStructLiteral(llb, module, (AST::StructLiteral*)expression);
	case AST::ExpressionType::Identifier:
		return GenIdentifier(llb, module, (AST::Identifier*)expression);
	case AST::ExpressionType::Compound:
		return GenCompoundExpression(llb, module, (AST::CompoundExpression*)expression);

	case AST::ExpressionType::FunctionCall:
		return GenFunctionCall(llb, module, (AST::FunctionCall*)expression);
	case AST::ExpressionType::SubscriptOperator:
		return GenSubscriptOperator(llb, module, (AST::SubscriptOperator*)expression);
	case AST::ExpressionType::DotOperator:
		return GenDotOperator(llb, module, (AST::DotOperator*)expression);
	case AST::ExpressionType::Typecast:
		return GenCast(llb, module, (AST::Typecast*)expression);
	case AST::ExpressionType::Sizeof:
		return GenSizeof(llb, module, (AST::Sizeof*)expression);
	case AST::ExpressionType::Malloc:
		return GenMalloc(llb, module, (AST::Malloc*)expression);

	case AST::ExpressionType::UnaryOperator:
		return GenUnaryOperator(llb, module, (AST::UnaryOperator*)expression);
	case AST::ExpressionType::BinaryOperator:
		return GenBinaryOperator(llb, module, (AST::BinaryOperator*)expression);
	case AST::ExpressionType::TernaryOperator:
		return GenTernaryOperator(llb, module, (AST::TernaryOperator*)expression);

	default:
		SnekAssert(false);
		return nullptr;
	}
}

bool BlockHasBranched(LLVMBackend* llb, SkModule* module)
{
	LLVMBasicBlockRef block = LLVM_CALL(LLVMGetInsertBlock, module->builder);
	if (LLVMValueRef lastInst = LLVM_CALL(LLVMGetLastInstruction, block)) {
		LLVMOpcode opCode = LLVM_CALL(LLVMGetInstructionOpcode, lastInst);
		return opCode == LLVMBr || opCode == LLVMRet;
	}
	return false;
}

static void GenCompoundStatement(LLVMBackend* llb, SkModule* module, AST::CompoundStatement* statement)
{
	for (int i = 0; i < statement->statements.size; i++)
	{
		GenStatement(llb, module, statement->statements[i]);
	}
}

static void GenVarDeclStatement(LLVMBackend* llb, SkModule* module, AST::VariableDeclaration* statement)
{
	LLVMTypeRef type = GenType(llb, module, statement->varType);
	for (int i = 0; i < statement->declarators.size; i++)
	{
		AST::VariableDeclarator* declarator = statement->declarators[i];

		const char* name = declarator->name;
		LLVMValueRef alloc = AllocateLocalVariable(llb, module, type, name);
		declarator->variable->allocHandle = alloc;

		if (declarator->value)
		{
			LLVMValueRef value = GenExpression(llb, module, declarator->value);
			value = ConvertAssignValue(llb, module, value, type, declarator->value->valueType, statement->varType->typeID, declarator->value->lvalue, LLVMIsConstant(value));
			LLVM_CALL(LLVMBuildStore, module->builder, value, alloc);
		}

		if (module->hasDebugInfo)
		{
			DebugInfoDeclareVariable(llb, module, alloc, statement->varType->typeID, name, statement->location);
		}

		declarator->variable->allocHandle = alloc;
	}
}

static void GenExprStatement(LLVMBackend* llb, SkModule* module, AST::ExpressionStatement* statement)
{
	LLVMValueRef value = GenExpression(llb, module, statement->expression);
}

static void GenIfStatement(LLVMBackend* llb, SkModule* module, AST::IfStatement* statement)
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

static void GenWhileLoop(LLVMBackend* llb, SkModule* module, AST::WhileLoop* statement)
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

static void GenForLoop(LLVMBackend* llb, SkModule* module, AST::ForLoop* statement)
{
	LLVMValueRef llvmFunction = module->functionValues[module->currentFunction];
	LLVMBasicBlockRef beforeBlock = LLVMGetInsertBlock(module->builder);
	LLVMBasicBlockRef headerBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "for.header");
	LLVMBasicBlockRef loopBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "for.loop");
	LLVMBasicBlockRef iterateBlock = LLVMCreateBasicBlockInContext(llb->llvmContext, "for.iterate");
	LLVMBasicBlockRef mergeBlock = LLVMCreateBasicBlockInContext(llb->llvmContext, "for.merge");

	statement->breakHandle = mergeBlock;
	statement->continueHandle = headerBlock;

	LLVMValueRef start = nullptr;
	LLVMValueRef end = nullptr;
	LLVMValueRef delta = nullptr;

	start = GenExpression(llb, module, statement->startValue);
	start = GetRValue(llb, module, start, statement->startValue->lvalue);
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(start)) == LLVMIntegerTypeKind);
	start = CastInt(llb, module, start, LLVMInt32TypeInContext(llb->llvmContext), statement->startValue->valueType);

	LLVMValueRef it = AllocateLocalVariable(llb, module, LLVMInt32TypeInContext(llb->llvmContext), statement->iteratorName);
	if (module->hasDebugInfo)
	{
		DebugInfoDeclareVariable(llb, module, it, GetIntegerType(32, true), statement->iteratorName, statement->location);
	}
	LLVM_CALL(LLVMBuildStore, module->builder, start, it);
	statement->iterator->allocHandle = it;

	LLVM_CALL(LLVMBuildBr, module->builder, headerBlock);
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, headerBlock);

	end = GenExpression(llb, module, statement->endValue);
	end = GetRValue(llb, module, end, statement->endValue->lvalue);
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(end)) == LLVMIntegerTypeKind);
	end = CastValue(llb, module, end, LLVMInt32TypeInContext(llb->llvmContext), statement->endValue->valueType, GetIntegerType(32, false));

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
	SnekAssert(statement->delta != 0);
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

	itValue = LLVM_CALL(LLVMBuildLoad, module->builder, it, "");
	LLVMValueRef nextItValue = LLVM_CALL(LLVMBuildAdd, module->builder, itValue, delta, "");
	LLVM_CALL(LLVMBuildStore, module->builder, nextItValue, it);
	LLVM_CALL(LLVMBuildBr, module->builder, headerBlock);

	LLVM_CALL(LLVMAppendExistingBasicBlock, llvmFunction, mergeBlock);
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, mergeBlock);
}

static void GenBreak(LLVMBackend* llb, SkModule* module, AST::Break* statement)
{
	if (statement->branchDst->type == AST::StatementType::While)
	{
		AST::WhileLoop* whileLoop = (AST::WhileLoop*)statement->branchDst;
		LLVMBasicBlockRef mergeBlock = (LLVMBasicBlockRef)whileLoop->breakHandle;
		LLVM_CALL(LLVMBuildBr, module->builder, mergeBlock);
	}
	else if (statement->branchDst->type == AST::StatementType::For)
	{
		AST::ForLoop* forLoop = (AST::ForLoop*)statement->branchDst;
		LLVMBasicBlockRef mergeBlock = (LLVMBasicBlockRef)forLoop->breakHandle;
		LLVM_CALL(LLVMBuildBr, module->builder, mergeBlock);
	}
	else
	{
		SnekAssert(false);
	}
}

static void GenContinue(LLVMBackend* llb, SkModule* module, AST::Continue* statement)
{
	if (statement->branchDst->type == AST::StatementType::While)
	{
		AST::WhileLoop* whileLoop = (AST::WhileLoop*)statement->branchDst;
		LLVMBasicBlockRef headerBlock = (LLVMBasicBlockRef)whileLoop->continueHandle;
		LLVM_CALL(LLVMBuildBr, module->builder, headerBlock);
	}
	else if (statement->branchDst->type == AST::StatementType::For)
	{
		AST::ForLoop* forLoop = (AST::ForLoop*)statement->branchDst;
		LLVMBasicBlockRef headerBlock = (LLVMBasicBlockRef)forLoop->continueHandle;
		LLVM_CALL(LLVMBuildBr, module->builder, headerBlock);
	}
	else
	{
		SnekAssert(false);
	}
}

static void GenReturn(LLVMBackend* llb, SkModule* module, AST::Return* statement)
{
	if (statement->value)
	{
		AST::Function* function = module->currentFunction;
		LLVMTypeRef returnType = LLVMGetReturnType(LLVMGetElementType(LLVMTypeOf((LLVMValueRef)function->valueHandle)));

		LLVMValueRef value = GenExpression(llb, module, statement->value);
		value = GetRValue(llb, module, value, statement->value->lvalue);
		value = CastValue(llb, module, value, returnType, statement->value->valueType, function->returnType->typeID);

		LLVM_CALL(LLVMBuildStore, module->builder, value, module->returnAlloc);
	}

	LLVM_CALL(LLVMBuildBr, module->builder, module->returnBlock);
}

static void GenFree(LLVMBackend* llb, SkModule* module, AST::Free* statement)
{
	for (int i = 0; i < statement->values.size; i++)
	{
		LLVMValueRef value = GenExpression(llb, module, statement->values[i]);
		value = GetRValue(llb, module, value, statement->values[i]->lvalue);

		if (statement->values[i]->valueType->typeKind == AST::TypeKind::Pointer)
		{
			LLVMBuildFree(module->builder, value);
		}
		else if (statement->values[i]->valueType->typeKind == AST::TypeKind::Class)
		{
			// TODO call destructor
			LLVMBuildFree(module->builder, value);
		}
	}
}

void GenStatement(LLVMBackend* llb, SkModule* module, AST::Statement* statement)
{
	if (module->hasDebugInfo)
	{
		DebugInfoEmitSourceLocation(llb, module, statement);
	}

	switch (statement->type)
	{
	case AST::StatementType::NoOp:
		break;
	case AST::StatementType::Compound:
		GenCompoundStatement(llb, module, (AST::CompoundStatement*)statement);
		break;
	case AST::StatementType::VariableDeclaration:
		GenVarDeclStatement(llb, module, (AST::VariableDeclaration*)statement);
		break;
	case AST::StatementType::Expression:
		GenExprStatement(llb, module, (AST::ExpressionStatement*)statement);
		break;
	case AST::StatementType::If:
		GenIfStatement(llb, module, (AST::IfStatement*)statement);
		break;
	case AST::StatementType::While:
		GenWhileLoop(llb, module, (AST::WhileLoop*)statement);
		break;
	case AST::StatementType::For:
		GenForLoop(llb, module, (AST::ForLoop*)statement);
		break;
	case AST::StatementType::Break:
		GenBreak(llb, module, (AST::Break*)statement);
		break;
	case AST::StatementType::Continue:
		GenContinue(llb, module, (AST::Continue*)statement);
		break;
	case AST::StatementType::Return:
		GenReturn(llb, module, (AST::Return*)statement);
		break;
	case AST::StatementType::Free:
		GenFree(llb, module, (AST::Free*)statement);
		break;

	default:
		SnekAssert(false);
		break;
	}
}

LLVMValueRef GenFunctionHeader(LLVMBackend* llb, SkModule* module, AST::Function* decl)
{
	if (decl->isGeneric)
	{
		return nullptr;
	}
	else
	{
		AST::Function* lastFunction = module->currentFunction;
		module->currentFunction = decl;

		LLVMValueRef llvmValue = nullptr;

		LLVMLinkage linkage = LLVMInternalLinkage;
		if (decl->isGenericInstance)
			linkage = LLVMPrivateLinkage;
		else if (HasFlag(decl->flags, AST::DeclarationFlags::Extern) || !decl->body)
			linkage = LLVMExternalLinkage;
		else if (HasFlag(decl->flags, AST::DeclarationFlags::DllExport))
			linkage = LLVMDLLExportLinkage;
		else if (HasFlag(decl->flags, AST::DeclarationFlags::DllImport))
			linkage = LLVMDLLImportLinkage;
		else if (decl->visibility == AST::Visibility::Private)
			linkage = LLVMPrivateLinkage;
		else if (decl->visibility == AST::Visibility::Public)
			linkage = LLVMExternalLinkage;

		LLVMTypeRef returnType = GenType(llb, module, decl->returnType);
		List<LLVMTypeRef> paramTypes = CreateList<LLVMTypeRef>();
		bool entryPoint = decl->isEntryPoint;

		int numParams = decl->paramTypes.size;
		for (int i = 0; i < numParams; i++)
		{
			LLVMTypeRef paramType = GenType(llb, module, decl->paramTypes[i]);
			paramTypes.add(paramType);
		}

		llvmValue = CreateFunction(llb, module, decl->mangledName, returnType, paramTypes, decl->varArgs, entryPoint, linkage, module->llvmModule);
		decl->valueHandle = llvmValue;

		module->functionValues.emplace(decl, llvmValue);

		module->currentFunction = lastFunction;

		return llvmValue;
	}
}

LLVMValueRef GenFunction(LLVMBackend* llb, SkModule* module, AST::Function* decl)
{
	if (decl->isGeneric)
	{
		return nullptr;
	}
	else
	{
		AST::Function* lastFunction = module->currentFunction;
		module->currentFunction = decl;

		LLVMValueRef llvmValue = nullptr;

		if (module->functionValues.find(decl) != module->functionValues.end())
			llvmValue = module->functionValues[decl];
		else
		{
			//llvmValue = GenFunctionHeader(llb, module, decl);
			SnekAssert(false);
		}

		if (decl->body)
		{
			GenerateFunctionBody(llb, module, decl, llvmValue);
		}

		module->currentFunction = lastFunction;

		return llvmValue;
	}
}

static LLVMTypeRef GenEnumHeader(LLVMBackend* llb, SkModule* module, AST::Enum* decl)
{
	return LLVMInt32TypeInContext(llb->llvmContext);
}

static LLVMTypeRef GenEnum(LLVMBackend* llb, SkModule* module, AST::Enum* decl)
{
	LLVMValueRef lastValue = NULL;
	for (int i = 0; i < decl->values.size; i++)
	{
		LLVMValueRef entryValue = NULL;
		if (decl->values[i]->value)
		{
			entryValue = GenExpression(llb, module, decl->values[i]->value);
			entryValue = CastInt(llb, module, entryValue, LLVMInt32TypeInContext(llb->llvmContext), decl->values[i]->value->valueType);
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
		decl->values[i]->valueHandle = entryValue;
	}

	return LLVMInt32TypeInContext(llb->llvmContext);
}

LLVMTypeRef GenStructHeader(LLVMBackend* llb, SkModule* module, AST::Struct* decl)
{
	if (decl->isGeneric)
	{
		return nullptr;
	}
	else
	{
		AST::Struct* lastStruct = module->currentStruct;
		module->currentStruct = decl;

		LLVMTypeRef llvmType = LLVM_CALL(LLVMStructCreateNamed, llb->llvmContext, decl->mangledName);
		decl->typeHandle = llvmType;

		llb->structTypes.emplace(decl, llvmType);

		module->currentStruct = lastStruct;

		return llvmType;
	}
}

LLVMTypeRef GenStruct(LLVMBackend* llb, SkModule* module, AST::Struct* decl)
{
	if (decl->isGeneric)
	{
		return nullptr;
	}
	else
	{
		AST::Struct* lastStruct = module->currentStruct;
		module->currentStruct = decl;

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
					fieldTypes[i] = GenType(llb, module, decl->fields[i]->type);
				}

				bool isPacked = HasFlag(decl->flags, AST::DeclarationFlags::Packed);

				LLVM_CALL(LLVMStructSetBody, llvmType, fieldTypes, numFields, isPacked);
			}
			else
			{
				LLVM_CALL(LLVMStructSetBody, llvmType, NULL, 0, false);
			}
		}

		module->currentStruct = lastStruct;

		return llvmType;
	}
}

static LLVMTypeRef GenClassHeader(LLVMBackend* llb, SkModule* module, AST::Class* decl)
{
	LLVMTypeRef llvmType = LLVM_CALL(LLVMStructCreateNamed, llb->llvmContext, decl->mangledName);
	decl->typeHandle = llvmType;

	llb->classTypes.emplace(decl, llvmType);

	return llvmType;
}

static LLVMValueRef GenClassMethodHeader(LLVMBackend* llb, SkModule* module, AST::Function* method, AST::Class* classDecl)
{
	AST::Function* lastFunction = module->currentFunction;
	module->currentFunction = method;

	LLVMTypeRef returnType = GenType(llb, module, method->returnType);
	int numParams = method->paramTypes.size + 1;
	List<LLVMTypeRef> paramTypes = CreateList<LLVMTypeRef>(numParams);
	paramTypes[0] = GenTypeID(llb, module, method->instanceVariable->type);
	for (int i = 0; i < method->paramTypes.size; i++)
	{
		paramTypes.add(GenType(llb, module, method->paramTypes[i]));
	}

	LLVMLinkage linkage = LLVMExternalLinkage;
	LLVMValueRef llvmValue = CreateFunction(llb, module, method->mangledName, returnType, paramTypes, method->varArgs, false, linkage, module->llvmModule);

	module->functionValues.emplace(method, llvmValue);

	method->valueHandle = llvmValue;

	module->currentFunction = lastFunction;

	return llvmValue;
}

static LLVMValueRef GenClassConstructorHeader(LLVMBackend* llb, SkModule* module, AST::Function* constructor, AST::Class* classDecl)
{
	AST::Function* lastFunction = module->currentFunction;
	module->currentFunction = constructor;

	LLVMTypeRef returnType = GenTypeID(llb, module, constructor->instanceVariable->type);
	int numParams = constructor->paramTypes.size + 1;
	List<LLVMTypeRef> paramTypes = CreateList<LLVMTypeRef>(numParams);
	paramTypes[0] = GenTypeID(llb, module, constructor->instanceVariable->type);
	for (int i = 0; i < constructor->paramTypes.size; i++)
	{
		paramTypes.add(GenType(llb, module, constructor->paramTypes[i]));
	}

	LLVMLinkage linkage = LLVMExternalLinkage;
	LLVMValueRef llvmValue = CreateFunction(llb, module, constructor->mangledName, returnType, paramTypes, constructor->varArgs, false, linkage, module->llvmModule);

	module->functionValues.emplace(constructor, llvmValue);

	constructor->valueHandle = llvmValue;

	module->currentFunction = lastFunction;

	return llvmValue;
}

static void GenClassProcedureHeaders(LLVMBackend* llb, SkModule* module, AST::Class* decl)
{
	for (int i = 0; i < decl->methods.size; i++)
	{
		GenClassMethodHeader(llb, module, decl->methods[i], decl);
	}
	if (decl->constructor)
		GenClassConstructorHeader(llb, module, decl->constructor, decl);
}

static LLVMTypeRef GenClass(LLVMBackend* llb, SkModule* module, AST::Class* decl)
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
			fieldTypes[i] = GenType(llb, module, decl->fields[i]->type);
		}

		LLVM_CALL(LLVMStructSetBody, llvmType, fieldTypes, numFields, false);
	}

	return llvmType;
}

static LLVMValueRef GenClassMethod(LLVMBackend* llb, SkModule* module, AST::Function* method, AST::Class* classDecl)
{
	AST::Function* lastFunction = module->currentFunction;
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
		DebugInfoBeginFunction(llb, module, method, llvmValue, method->instanceVariable->type);
		DebugInfoEmitNullLocation(llb, module, module->builder);
	}

	LLVMTypeRef returnType = LLVM_CALL(LLVMGetReturnType, LLVMGetElementType(LLVMTypeOf(llvmValue)));

	LLVMBasicBlockRef parentBlock = LLVM_CALL(LLVMGetInsertBlock, module->builder);
	LLVMBasicBlockRef entryBlock = LLVM_CALL(LLVMAppendBasicBlockInContext, llb->llvmContext, llvmValue, "entry");
	LLVMBasicBlockRef returnBlock = LLVM_CALL(LLVMCreateBasicBlockInContext, llb->llvmContext, "return");
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, entryBlock);

	LLVMValueRef instance = LLVM_CALL(LLVMGetParam, llvmValue, 0);
	LLVMValueRef instanceAlloc = AllocateLocalVariable(llb, module, GenTypeID(llb, module, method->instanceVariable->type), "this");
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
	module->returnAlloc = method->returnType->typeKind != AST::TypeKind::Void ? AllocateLocalVariable(llb, module, returnType, "__return") : NULL;

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

static LLVMValueRef GenClassConstructor(LLVMBackend* llb, SkModule* module, AST::Function* constructor, AST::Class* classDecl)
{
	AST::Function* lastFunction = module->currentFunction;
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
		DebugInfoBeginFunction(llb, module, constructor, llvmValue, constructor->instanceVariable->type);
		DebugInfoEmitNullLocation(llb, module, module->builder);
	}

	LLVMTypeRef returnType = LLVM_CALL(LLVMGetReturnType, LLVMGetElementType(LLVMTypeOf(llvmValue)));

	LLVMBasicBlockRef parentBlock = LLVM_CALL(LLVMGetInsertBlock, module->builder);
	LLVMBasicBlockRef entryBlock = LLVM_CALL(LLVMAppendBasicBlockInContext, llb->llvmContext, llvmValue, "entry");
	LLVMBasicBlockRef returnBlock = LLVM_CALL(LLVMCreateBasicBlockInContext, llb->llvmContext, "return");
	LLVM_CALL(LLVMPositionBuilderAtEnd, module->builder, entryBlock);

	LLVMValueRef instance = LLVM_CALL(LLVMGetParam, llvmValue, 0);
	LLVMValueRef instanceAlloc = AllocateLocalVariable(llb, module, GenTypeID(llb, module, constructor->instanceVariable->type), "this");
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

static void GenClassProcedures(LLVMBackend* llb, SkModule* module, AST::Class* decl)
{
	for (int i = 0; i < decl->methods.size; i++)
	{
		GenClassMethod(llb, module, decl->methods[i], decl);
	}
	if (decl->constructor)
		GenClassConstructor(llb, module, decl->constructor, decl);
}

static LLVMValueRef GenGlobalHeader(LLVMBackend* llb, SkModule* module, AST::GlobalVariable* decl)
{
	SnekAssert(decl->declarators.size == 1);

	LLVMTypeRef llvmType = GenType(llb, module, decl->type);

	AST::VariableDeclarator* declarator = decl->declarators[0];
	LLVMValueRef alloc = LLVMAddGlobal(module->llvmModule, llvmType, declarator->name);

	bool isConstant = HasFlag(decl->flags, AST::DeclarationFlags::Constant);
	if (isConstant)
	{
		LLVM_CALL(LLVMSetGlobalConstant, alloc, true);
		LLVM_CALL(LLVMSetLinkage, alloc, LLVMPrivateLinkage);
	}
	else
	{
		LLVMLinkage linkage = LLVMInternalLinkage;
		if (HasFlag(decl->flags, AST::DeclarationFlags::Extern))
			linkage = LLVMExternalLinkage;
		else if (HasFlag(decl->flags, AST::DeclarationFlags::DllExport))
			linkage = LLVMDLLExportLinkage;
		else if (HasFlag(decl->flags, AST::DeclarationFlags::DllImport))
			linkage = LLVMDLLImportLinkage;
		else if (declarator->variable->visibility == AST::Visibility::Private)
			linkage = LLVMPrivateLinkage;
		else if (declarator->variable->visibility == AST::Visibility::Public)
			linkage = LLVMExternalLinkage;

		LLVM_CALL(LLVMSetLinkage, alloc, linkage);
	}

	declarator->variable->allocHandle = alloc;
	module->globalValues.emplace(decl, alloc);

	return alloc;
}

static void GenGlobal(LLVMBackend* llb, SkModule* module, AST::GlobalVariable* global)
{
	SnekAssert(global->declarators.size == 1);

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

	bool isExtern = HasFlag(global->flags, AST::DeclarationFlags::Extern);

	AST::VariableDeclarator* declarator = global->declarators[0];
	if (declarator->value)
	{
		LLVMValueRef value = GenExpression(llb, module, declarator->value);
		value = GetRValue(llb, module, value, declarator->value->lvalue);
		value = ConstCastValue(llb, module, value, llvmType, declarator->value->valueType, global->type->typeID);

		/*
		if (global->value->exprKind == AST::ExpressionType::STRING_LITERAL)
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
		DebugInfoDeclareGlobal(llb, module, alloc, declarator->name, declarator->name, global->type->typeID, declarator->location);
	}
}

static void GenImport(LLVMBackend* llb, SkModule* module, AST::Import* decl)
{
}

static void GenBuiltInDecls(LLVMBackend* llb, SkModule* module)
{
}

static SkModule** GenModules(LLVMBackend* llb, int numModules, AST::File** asts, bool genDebugInfo)
{
	SkModule** modules = new SkModule * [numModules];

	for (int i = 0; i < numModules; i++)
	{
		AST::File* ast = asts[i];
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

static void RetrieveModuleString(AST::Module* module, AST::File* file, std::stringstream& stream)
{
	if (module->parent)
	{
		RetrieveModuleString(module->parent, file, stream);
		if (!(strcmp(file->name, module->name) == 0 && file->module == module))
			stream << module->name << '.';
	}
}

static void OutputModule(LLVMBackend* llb, AST::File* file, LLVMModuleRef llvmModule, const char* buildFolder, bool emitLLVM, List<LinkerFile>& outFiles)
{
	if (emitLLVM)
	{
		std::stringstream filename;
		filename << buildFolder << '/';
		RetrieveModuleString(file->module, file, filename);
		filename << file->name << ".ll";

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
		RetrieveModuleString(file->module, file, filename);
		filename << file->name << ".obj";

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

bool LLVMBackendCompile(LLVMBackend* llb, AST::File** asts, int numModules, const char* filename, const char* buildFolder, bool genDebugInfo, bool emitLLVM, int optLevel)
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

			printf("\n\n\n\n%s\n\n\n\n", LLVMPrintModuleToString(llvmModule));

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
