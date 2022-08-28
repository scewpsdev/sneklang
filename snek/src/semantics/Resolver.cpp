#include "Resolver.h"

#include "Snek.h"
#include "Variable.h"
#include "Type.h"
#include "Mangling.h"
#include "utils/Utils.h"

#include <math.h>
#include <stddef.h>


static AST::Module* CreateModule(Resolver* resolver, const char* name, AST::Module* parent)
{
	AST::Module* module = new AST::Module(name, parent);

	if (parent)
		parent->children.add(module);

	return module;
}

static AST::Module* FindModule(Resolver* resolver, const char* name, AST::Module* parent)
{
	for (int i = 0; i < parent->children.size; i++)
	{
		if (strcmp(name, parent->children[i]->name) == 0)
			return parent->children[i];
	}
	return nullptr;
}

static AST::Module* FindModuleInDependencies(Resolver* resolver, const char* name, AST::File* file)
{
	for (int i = 0; i < file->dependencies.size; i++)
	{
		if (strcmp(name, file->dependencies[i]->name) == 0)
			return file->dependencies[i];
	}
	return nullptr;
}

static AST::File* FindFileWithNamespace(Resolver* resolver, const char* name, AST::File* parent)
{
	for (int i = 0; i < parent->dependencies.size; i++)
	{
		AST::Module* dependency = parent->dependencies[i];
		for (AST::File* file : dependency->files)
		{
			if (file->nameSpace && strcmp(file->nameSpace, name) == 0)
				return file;
		}
	}
	return nullptr;
}

static bool ResolveType(Resolver* resolver, AST::Type* type);
static bool ResolveExpression(Resolver* resolver, AST::Expression* expr);
static bool ResolveFunctionHeader(Resolver* resolver, AST::Function* decl);
static bool ResolveFunction(Resolver* resolver, AST::Function* decl);
static bool ResolveStructHeader(Resolver* resolver, AST::Struct* decl);
static bool ResolveStruct(Resolver* resolver, AST::Struct* decl);

static bool IsPrimitiveType(AST::TypeKind typeKind)
{
	return typeKind == AST::TypeKind::Integer
		|| typeKind == AST::TypeKind::FloatingPoint
		|| typeKind == AST::TypeKind::Boolean;
}

static int64_t ConstantFoldInt(Resolver* resolver, AST::Expression* expr, bool& success)
{
	switch (expr->type)
	{
	case AST::ExpressionType::IntegerLiteral:
		return ((AST::IntegerLiteral*)expr)->value;
	case AST::ExpressionType::Identifier:
	{
		Variable* variable = ((AST::Identifier*)expr)->variable;
		if (!variable)
		{
			SnekError(resolver->context, expr->location, ERROR_CODE_CONSTANT_INITIALIZER_NON_CONSTANT, "Variable '%s' must be defined before using it to initialize a constant", ((AST::Identifier*)expr)->name);
			success = false;
			return 0;
		}
		else
		{
			SnekAssert(variable->isConstant);
			return ConstantFoldInt(resolver, variable->value, success);
		}
	}
	case AST::ExpressionType::Compound:
		return ConstantFoldInt(resolver, ((AST::CompoundExpression*)expr)->value, success);

	case AST::ExpressionType::UnaryOperator:
	{
		AST::UnaryOperator* unaryOperator = (AST::UnaryOperator*)expr;
		switch (unaryOperator->operatorType)
		{
		case AST::UnaryOperatorType::Negate:
			return -ConstantFoldInt(resolver, unaryOperator->operand, success);

		default:
			SnekAssert(false);
			return 0;
		}
	}
	case AST::ExpressionType::BinaryOperator:
	{
		AST::BinaryOperator* binaryOperator = (AST::BinaryOperator*)expr;
		switch (binaryOperator->operatorType)
		{
		case AST::BinaryOperatorType::Add:
			return ConstantFoldInt(resolver, binaryOperator->left, success) + ConstantFoldInt(resolver, binaryOperator->right, success);
		case AST::BinaryOperatorType::Subtract:
			return ConstantFoldInt(resolver, binaryOperator->left, success) - ConstantFoldInt(resolver, binaryOperator->right, success);
		case AST::BinaryOperatorType::Multiply:
			return ConstantFoldInt(resolver, binaryOperator->left, success) * ConstantFoldInt(resolver, binaryOperator->right, success);
		case AST::BinaryOperatorType::Divide:
			return ConstantFoldInt(resolver, binaryOperator->left, success) / ConstantFoldInt(resolver, binaryOperator->right, success);
		case AST::BinaryOperatorType::Modulo:
			return ConstantFoldInt(resolver, binaryOperator->left, success) % ConstantFoldInt(resolver, binaryOperator->right, success);

		case AST::BinaryOperatorType::BitwiseAnd:
			return ConstantFoldInt(resolver, binaryOperator->left, success) & ConstantFoldInt(resolver, binaryOperator->right, success);
		case AST::BinaryOperatorType::BitwiseOr:
			return ConstantFoldInt(resolver, binaryOperator->left, success) | ConstantFoldInt(resolver, binaryOperator->right, success);
		case AST::BinaryOperatorType::BitwiseXor:
			return ConstantFoldInt(resolver, binaryOperator->left, success) ^ ConstantFoldInt(resolver, binaryOperator->right, success);
		case AST::BinaryOperatorType::BitshiftLeft:
			return ConstantFoldInt(resolver, binaryOperator->left, success) << ConstantFoldInt(resolver, binaryOperator->right, success);
		case AST::BinaryOperatorType::BitshiftRight:
			return ConstantFoldInt(resolver, binaryOperator->left, success) >> ConstantFoldInt(resolver, binaryOperator->right, success);

		default:
			SnekAssert(false);
			return 0;
		}
	}

	default:
		SnekAssert(false);
		return 0;
	}
}

static bool ResolveVoidType(Resolver* resolver, AST::VoidType* type)
{
	if (type->typeID = GetVoidType())
		return true;
	return false;
}

static bool ResolveIntegerType(Resolver* resolver, AST::IntegerType* type)
{
	if (type->typeID = GetIntegerType(type->bitWidth, type->isSigned))
		return true;
	return false;
}

static bool ResolveFPType(Resolver* resolver, AST::FloatingPointType* type)
{
	switch (type->bitWidth)
	{
	case 16:
		type->typeID = GetFloatingPointType(FloatingPointPrecision::Half);
		return true;
	case 32:
		type->typeID = GetFloatingPointType(FloatingPointPrecision::Single);
		return true;
	case 64:
		type->typeID = GetFloatingPointType(FloatingPointPrecision::Double);
		return true;
	case 128:
		type->typeID = GetFloatingPointType(FloatingPointPrecision::Quad);
		return true;
	default:
		SnekAssert(false);
		return false;
	}
}

static bool ResolveBoolType(Resolver* resolver, AST::BooleanType* type)
{
	if (type->typeID = GetBoolType())
		return true;
	return false;
}

static bool ResolveNamedType(Resolver* resolver, AST::NamedType* type)
{
	if (AST::Struct* structDecl = FindStruct(resolver, type->name))
	{
		if (structDecl->isGeneric)
		{
			if (!type->hasGenericArgs)
			{
				SnekError(resolver->context, type->location, ERROR_CODE_STRUCT_SYNTAX, "Using generic struct type '%s' without type arguments", structDecl->name);
				return false;
			}

			bool result = true;

			for (int i = 0; i < type->genericArgs.size; i++)
			{
				result = ResolveType(resolver, type->genericArgs[i]) && result;
			}

			AST::Struct* instance = structDecl->getGenericInstance(type->genericArgs);
			if (!instance)
			{
				instance = (AST::Struct*)structDecl->copy();
				instance->isGeneric = false;
				instance->isGenericInstance = true;

				instance->genericTypeArguments.resize(type->genericArgs.size);
				for (int i = 0; i < type->genericArgs.size; i++)
				{
					instance->genericTypeArguments[i] = type->genericArgs[i]->typeID;
				}

				result = ResolveStructHeader(resolver, instance) && result;
				result = ResolveStruct(resolver, instance) && result;

				structDecl->genericInstances.add(instance);
			}

			type->typeKind = AST::TypeKind::Alias;
			type->typeID = instance->type;
			type->declaration = instance;

			return result;
		}
		else
		{
			if (type->hasGenericArgs)
			{
				SnekError(resolver->context, type->location, ERROR_CODE_STRUCT_SYNTAX, "Can't use type arguments on non-generic struct type '%s'", structDecl->name);
				return false;
			}

			type->typeKind = AST::TypeKind::Struct;
			type->typeID = structDecl->type;
			type->declaration = structDecl;
			return true;
		}
	}
	else if (AST::Class* classDecl = FindClass(resolver, type->name))
	{
		type->typeKind = AST::TypeKind::Class;
		type->typeID = classDecl->type;
		type->declaration = classDecl;
		return true;
	}
	else if (AST::Typedef* typedefDecl = FindTypedef(resolver, type->name))
	{
		type->typeKind = AST::TypeKind::Alias;
		type->typeID = typedefDecl->type;
		type->declaration = typedefDecl;
		return true;
	}
	else if (AST::Enum* enumDecl = FindEnum(resolver, type->name))
	{
		type->typeKind = AST::TypeKind::Alias;
		type->typeID = enumDecl->type;
		type->declaration = enumDecl;
		return true;
	}
	else if (TypeID genericTypeArgument = resolver->getGenericTypeArgument(type->name))
	{
		type->typeKind = genericTypeArgument->typeKind;
		type->typeID = genericTypeArgument;
		return true;
	}

	SnekError(resolver->context, type->location, ERROR_CODE_UNDEFINED_TYPE, "Undefined type '%s'", type->name);
	return false;
}

static bool ResolvePointerType(Resolver* resolver, AST::PointerType* type)
{
	if (ResolveType(resolver, type->elementType))
	{
		type->typeID = GetPointerType(type->elementType->typeID);
		return true;
	}
	else
	{
		return false;
	}
}

static bool ResolveFunctionType(Resolver* resolver, AST::FunctionType* type)
{
	bool result = true;

	result = ResolveType(resolver, type->returnType) && result;

	for (int i = 0; i < type->paramTypes.size; i++)
	{
		result = ResolveType(resolver, type->paramTypes[i]) && result;
	}

	TypeID returnType = type->returnType->typeID;
	int numParams = type->paramTypes.size;
	TypeID* paramTypes = new TypeID[numParams];
	bool varArgs = type->varArgs;
	for (int i = 0; i < numParams; i++)
	{
		paramTypes[i] = type->paramTypes[i]->typeID;
	}

	type->typeID = GetFunctionType(type->returnType->typeID, numParams, paramTypes, varArgs, false, nullptr, nullptr);

	return result;
}

static bool ResolveArrayType(Resolver* resolver, AST::ArrayType* type)
{
	if (ResolveType(resolver, type->elementType))
	{
		if (type->length)
		{
			if (ResolveExpression(resolver, type->length))
			{
				if (type->length->valueType->typeKind == AST::TypeKind::Integer && type->length->isConstant())
				{
					bool success = true;
					type->typeID = GetArrayType(type->elementType->typeID, (int)ConstantFoldInt(resolver, type->length, success));
					return success;
				}
				else
				{
					bool success = true;
					type->typeID = GetArrayType(type->elementType->typeID, -1);
					return success;

					//SnekError(resolver->context, type->length->location, ERROR_CODE_ARRAY_LENGTH_WRONG_TYPE, "Array length specifier must be a constant integer value");
					//return false;
				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			type->typeID = GetArrayType(type->elementType->typeID, -1);
			return true;
		}
	}
	else
	{
		return false;
	}
}

static bool ResolveStringType(Resolver* resolver, AST::StringType* type)
{
	if (type->length)
	{
		if (ResolveExpression(resolver, type->length))
		{
			//if (type->length->valueType->typeKind == AST::TypeKind::Integer && type->length->isConstant())
			//{
			bool success = true;
			//type->typeID = GetStringType((int)ConstantFoldInt(resolver, type->length, success));
			type->typeID = GetStringType();
			return success;
			//}
			//else
			//{
			//	SnekError(resolver->context, type->length->location, ERROR_CODE_ARRAY_LENGTH_WRONG_TYPE, "String length specifier must be a constant integer value");
			//	return false;
			//}
		}
		else
		{
			return false;
		}
	}
	else
	{
		type->typeID = GetStringType();
		return true;
	}
}

static bool ResolveType(Resolver* resolver, AST::Type* type)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = type;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	switch (type->typeKind)
	{
	case AST::TypeKind::Void:
		return ResolveVoidType(resolver, (AST::VoidType*)type);
	case AST::TypeKind::Integer:
		return ResolveIntegerType(resolver, (AST::IntegerType*)type);
	case AST::TypeKind::FloatingPoint:
		return ResolveFPType(resolver, (AST::FloatingPointType*)type);
	case AST::TypeKind::Boolean:
		return ResolveBoolType(resolver, (AST::BooleanType*)type);
	case AST::TypeKind::NamedType:
		return ResolveNamedType(resolver, (AST::NamedType*)type);
	case AST::TypeKind::Pointer:
		return ResolvePointerType(resolver, (AST::PointerType*)type);
	case AST::TypeKind::Function:
		return ResolveFunctionType(resolver, (AST::FunctionType*)type);

	case AST::TypeKind::Array:
		return ResolveArrayType(resolver, (AST::ArrayType*)type);
	case AST::TypeKind::String:
		return ResolveStringType(resolver, (AST::StringType*)type);

	default:
		SnekAssert(false);
		return false;
	}
}

static TypeID GetFittingTypeForIntegerLiteral(Resolver* resolver, AST::IntegerLiteral* expr)
{
	if (expr->value < 0)
	{
		if (expr->value >= INT8_MIN)
			return GetIntegerType(8, true);
		else if (expr->value >= INT16_MIN)
			return GetIntegerType(16, true);
		else if (expr->value >= INT32_MIN)
			return GetIntegerType(32, true);
		else if (expr->value >= INT64_MIN)
			return GetIntegerType(64, true);
		else
		{
			SnekAssert(false);
			return nullptr;
		}
	}
	else
	{
		if (expr->value <= UINT8_MAX)
			return GetIntegerType(8, false);
		else if (expr->value <= UINT16_MAX)
			return GetIntegerType(16, false);
		else if (expr->value <= UINT32_MAX)
			return GetIntegerType(32, false);
		else if (expr->value <= UINT64_MAX)
			return GetIntegerType(64, false);
		else
		{
			SnekAssert(false);
			return nullptr;
		}
	}
}

static bool ResolveIntegerLiteral(Resolver* resolver, AST::IntegerLiteral* expr)
{
	expr->valueType = GetFittingTypeForIntegerLiteral(resolver, expr);
	expr->lvalue = false;
	return true;
}

static bool ResolveFPLiteral(Resolver* resolver, AST::FloatingPointLiteral* expr)
{
	expr->valueType = GetFloatingPointType(FloatingPointPrecision::Single);
	expr->lvalue = false;
	return true;
}

static bool ResolveBoolLiteral(Resolver* resolver, AST::BooleanLiteral* expr)
{
	expr->valueType = GetBoolType();
	expr->lvalue = false;
	return true;
}

static bool ResolveCharacterLiteral(Resolver* resolver, AST::CharacterLiteral* expr)
{
	expr->valueType = GetIntegerType(8, false);
	expr->lvalue = false;
	return true;
}

static bool ResolveNullLiteral(Resolver* resolver, AST::NullLiteral* expr)
{
	expr->valueType = GetPointerType(GetVoidType());
	expr->lvalue = false;
	return true;
}

static bool ResolveStringLiteral(Resolver* resolver, AST::StringLiteral* expr)
{
	//expr->valueType = GetStringType(expr->length + 1);
	expr->valueType = GetStringType();
	expr->lvalue = true;
	return true;
}

static bool ResolveInitializerList(Resolver* resolver, AST::InitializerList* expr)
{
	bool result = true;

	//if (ResolveType(resolver, expr->structType))
	{
		TypeID* valueTypes = new TypeID[expr->values.size];
		for (int i = 0; i < expr->values.size; i++)
		{
			if (ResolveExpression(resolver, expr->values[i]))
			{
				//if (IsConstant(expr->values[i]))
				//{
				valueTypes[i] = expr->values[i]->valueType;
				//}
				//else
				//{
					// ERROR
				//	result = false;
				//}
			}
			else
			{
				result = false;
			}
		}

		if (expr->initializerTypeAST)
		{
			result = ResolveType(resolver, expr->initializerTypeAST) && result;
			expr->initializerType = expr->initializerTypeAST->typeID;
		}
		else
		{
			SnekAssert(expr->values.size > 0);
			expr->initializerType = GetArrayType(expr->values[0]->valueType, expr->values.size);
		}

		expr->valueType = expr->initializerType;
		expr->lvalue = false;
	}

	return result;
}

static bool ResolveIdentifier(Resolver* resolver, AST::Identifier* expr)
{
	if (Variable* variable = resolver->findVariable(expr->name))
	{
		expr->valueType = variable->type;
		expr->lvalue = true; // !variable->isConstant;
		expr->variable = variable;
		return true;
	}
	if (resolver->findFunctions(expr->name, expr->functions))
	{
		expr->valueType = expr->functions[0]->functionType;
		expr->lvalue = false;
		//expr->function = function;
		return true;
	}
	if (AST::EnumValue* enumValue = FindEnumValue(resolver, expr->name))
	{
		expr->valueType = enumValue->declaration->type;
		expr->lvalue = false;
		expr->enumValue = enumValue;
		return true;
	}
	if (AST::Exprdef* exprdef = FindExprdef(resolver, expr->name))
	{
		AST::Expression* value = (AST::Expression*)exprdef->alias->copy();
		value->file = resolver->currentFile;

		if (ResolveExpression(resolver, value))
		{
			expr->valueType = value->valueType;
			expr->lvalue = value->lvalue;
			expr->exprdefValue = value;
			return true;
		}
	}

	SnekError(resolver->context, expr->location, ERROR_CODE_UNRESOLVED_IDENTIFIER, "Unresolved identifier '%s'", expr->name);
	return false;
}

static bool ResolveCompoundExpression(Resolver* resolver, AST::CompoundExpression* expr)
{
	if (ResolveExpression(resolver, expr->value))
	{
		expr->valueType = expr->value->valueType;
		expr->lvalue = expr->value->lvalue;
		return true;
	}
	return false;
}

static bool ResolveFunctionCall(Resolver* resolver, AST::FunctionCall* expr)
{
	bool result = true;

	TypeID functionType = NULL;
	AST::Function* function = NULL;

	if (ResolveExpression(resolver, expr->callee))
	{
		for (int i = 0; i < expr->arguments.size; i++)
		{
			if (!ResolveExpression(resolver, expr->arguments[i]))
				result = false;
		}
		if (!result)
			return false;


		const char* functionName = nullptr;
		List<AST::Function*>* functionOverloads = nullptr;
		if (expr->callee->type == AST::ExpressionType::Identifier && ((AST::Identifier*)expr->callee)->functions.size > 0)
		{
			functionName = ((AST::Identifier*)expr->callee)->name;
			functionOverloads = &((AST::Identifier*)expr->callee)->functions;
		}
		else if (expr->callee->type == AST::ExpressionType::DotOperator && ((AST::DotOperator*)expr->callee)->namespacedFunctions.size > 0)
		{
			functionName = ((AST::DotOperator*)expr->callee)->name;
			functionOverloads = &((AST::DotOperator*)expr->callee)->namespacedFunctions;
		}

		if (functionOverloads)
		{
			resolver->chooseFunctionOverload(*functionOverloads, expr->arguments);

			if (functionOverloads->size > 0)
			{
				int lowestScore = resolver->getFunctionOverloadScore(functionOverloads->get(0), expr->arguments);

				if (functionOverloads->size == 1)
				{
					expr->callee->valueType = functionOverloads->get(0)->functionType;
				}
				else if (functionOverloads->size > 1 && resolver->getFunctionOverloadScore(functionOverloads->get(1), expr->arguments) > lowestScore)
				{
					expr->callee->valueType = functionOverloads->get(0)->functionType;
				}
				else
				{
					char argumentTypesString[128] = "";
					for (int i = 0; i < expr->arguments.size; i++)
					{
						strcat(argumentTypesString, GetTypeString(expr->arguments[i]->valueType));
						if (i < expr->arguments.size - 1)
							strcat(argumentTypesString, ",");
					}

					char functionList[1024] = "";
					for (int i = 0; i < functionOverloads->size; i++)
					{
						AST::Function* function = (*functionOverloads)[i];

						const char* functionTypeString = GetTypeString(function->functionType);
						strcat(functionList, function->name);
						strcat(functionList, functionTypeString);

						if (i < functionOverloads->size - 1)
							strcat(functionList, ", ");
					}

					SnekError(resolver->context, expr->callee->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Ambiguous function call %s(%s): %i possible overloads: %s", functionName, argumentTypesString, functionOverloads->size, functionList);
					result = false;
				}
			}
			else
			{
				char argumentTypesString[128] = "";
				for (int i = 0; i < expr->arguments.size; i++)
				{
					strcat(argumentTypesString, GetTypeString(expr->arguments[i]->valueType));
					if (i < expr->arguments.size - 1)
						strcat(argumentTypesString, ",");
				}

				SnekError(resolver->context, expr->callee->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "No overload for function call %s(%s)", functionName, argumentTypesString);
				result = false;
			}
		}

		if (!result)
			return false;


		if (expr->hasGenericArgs)
		{
			SnekAssert(expr->callee->type == AST::ExpressionType::Identifier);

			for (int i = 0; i < expr->genericArgs.size; i++)
			{
				result = ResolveType(resolver, expr->genericArgs[i]) && result;
			}

			AST::Identifier* calleeIdentifier = (AST::Identifier*)expr->callee;

			function = calleeIdentifier->functions[0]->getGenericInstance(expr->genericArgs);
			if (!function)
			{
				function = (AST::Function*)(calleeIdentifier->functions[0]->copy()); // Create a separate version of the function, TODO: reuse functions with the same type arguments
				function->isGeneric = false;
				function->isGenericInstance = true;

				function->genericTypeArguments.resize(expr->genericArgs.size);
				for (int i = 0; i < expr->genericArgs.size; i++)
				{
					function->genericTypeArguments[i] = expr->genericArgs[i]->typeID;
				}

				result = ResolveFunctionHeader(resolver, function) && result;
				result = ResolveFunction(resolver, function) && result;

				functionType = function->functionType;

				calleeIdentifier->functions[0]->genericInstances.add(function);

				/*
				// Create function type for generic arguments

				TypeID returnType = function->returnType->typeID;
				int numParams = function->paramTypes.size;
				TypeID* paramTypes = new TypeID[numParams];

				for (int i = 0; i < numParams; i++)
				{
					paramTypes[i] = function->paramTypes[i]->typeID;
				}

				functionType = GetFunctionType(returnType, numParams, paramTypes, function->varArgs, false, function);
				*/
			}
		}
		else
		{
			if (expr->callee->type == AST::ExpressionType::Identifier)
			{
				AST::Identifier* calleeIdentifier = (AST::Identifier*)expr->callee;
				if (calleeIdentifier->functions.size > 0 && calleeIdentifier->functions[0]->isGeneric)
				{
					SnekError(resolver->context, expr->callee->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Calling generic function '%s' without type arguments", calleeIdentifier->functions[0]->name);
					return false;
				}
			}
			else if (expr->callee->type == AST::ExpressionType::DotOperator)
			{
				AST::DotOperator* calleeDotOperator = (AST::DotOperator*)expr->callee;
				if (calleeDotOperator->namespacedFunctions.size > 0 && calleeDotOperator->namespacedFunctions[0]->isGeneric)
				{
					// HACK
					if (calleeDotOperator->operand->type == AST::ExpressionType::Identifier)
					{
						SnekError(resolver->context, expr->callee->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Calling generic function '%s'.'%s' without type arguments", ((AST::Identifier*)calleeDotOperator->operand)->name, calleeDotOperator->namespacedFunctions[0]->name);
						return false;
					}
					else
					{
						SnekError(resolver->context, expr->callee->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Calling generic function '%s' without type arguments", calleeDotOperator->namespacedFunctions[0]->name);
						return false;
					}
				}
			}

			functionType = UnwrapType(expr->callee->valueType);

			if (functionType->typeKind != AST::TypeKind::Function)
			{
				SnekError(resolver->context, expr->callee->location, ERROR_CODE_NON_INVOKABLE_EXPRESSION, "Can't invoke expression of type %s", GetTypeString(expr->callee->valueType));
				return false;
			}

			function = functionType->functionType.declaration;
		}

		expr->function = function;
		expr->valueType = functionType->functionType.returnType;
		expr->lvalue = false;

		if (expr->callee->type == AST::ExpressionType::DotOperator)
		{
			AST::DotOperator* dotOperator = (AST::DotOperator*)expr->callee;
			if (dotOperator->classMethod)
			{
				expr->isMethodCall = true;
				expr->methodInstance = ((AST::DotOperator*)expr->callee)->operand;

				if (!expr->methodInstance->lvalue && expr->methodInstance->valueType->typeKind != AST::TypeKind::Pointer)
				{
					SnekError(resolver->context, expr->location, ERROR_CODE_RETURN_WRONG_TYPE, "Instance for method call must be an lvalue or a reference");
					result = false;
				}

				/*
				if (expr->arguments.size < functionType->functionType.numParams - 1 || expr->arguments.size > functionType->functionType.numParams - 1 && !functionType->functionType.varArgs)
				{
					if (function)
						SnekError(resolver->context, expr->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments when calling function %s: should be %d instead of %d", function->name, functionType->functionType.numParams - 1, expr->arguments.size);
					else
						SnekError(resolver->context, expr->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments in function call: should be %d instead of %d", functionType->functionType.numParams - 1, expr->arguments.size);
					result = false;
				}
				*/
			}
			//else if (dotOperator->namespacedFunction)
			//{
				//expr->function = dotOperator->namespacedFunction;
				//expr->valueType = dotOperator->namespacedFunction->returnType->typeID;
				//expr->lvalue = false;
				//expr->isMethodCall = false;

				//functionType = dotOperator->namespacedFunction->type;
				//functionAst = dotOperator->namespacedFunction;
			//}
		}

		if (expr->arguments.size < (function ? function->getNumRequiredParams() : functionType->functionType.numParams) ||
			expr->arguments.size > functionType->functionType.numParams && !functionType->functionType.varArgs)
		{
			if (function)
				SnekError(resolver->context, expr->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments when calling function '%s': should be %d instead of %d", function->name, functionType->functionType.numParams, expr->arguments.size);
			else
				SnekError(resolver->context, expr->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments in function call: should be %d instead of %d", functionType->functionType.numParams, expr->arguments.size);
			result = false;
		}
	}
	else
	{
		result = false;
	}

	/*
	if (expr->calleeExpr->exprKind == AST::ExpressionType::IDENTIFIER)
	{
		if (AstFunction* function = FindFunction(resolver, ((AstIdentifier*)expr->calleeExpr)->name))
		{
			if (expr->arguments.size < function->paramTypes.size || expr->arguments.size > function->paramTypes.size && !function->varArgs)
			{
				SnekError(resolver->context, expr->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments in function call: should be %d instead of %d", function->paramTypes.size, expr->arguments.size);
				result = false;
			}

			expr->function = function;
			expr->valueType = function->returnType->typeID;
			expr->lvalue = false;

			functionType = function->type;
			functionAst = function;
		}
	}
	else if (expr->calleeExpr->exprKind == AST::ExpressionType::DOT_OPERATOR)
	{
		if (ResolveExpression(resolver, expr->calleeExpr))
		{
			AstDotOperator* dotOperator = (AstDotOperator*)expr->calleeExpr;
			if (dotOperator->classMethod)
			{
				AstFunction* method = dotOperator->classMethod;
				if (expr->arguments.size < method->paramTypes.size || expr->arguments.size > method->paramTypes.size && !method->varArgs)
				{
					SnekError(resolver->context, expr->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments in method call: should be %d instead of %d", method->paramTypes.size, expr->arguments.size);
					result = false;
				}

				expr->function = dotOperator->classMethod;
				expr->valueType = dotOperator->classMethod->returnType->typeID;
				expr->lvalue = false;
				expr->isMethodCall = true;
				expr->methodInstance = dotOperator->operand;

				functionType = dotOperator->classMethod->type;
				functionAst = dotOperator->classMethod;
			}
			else if (dotOperator->nameSpace)
			{
				expr->function = dotOperator->namespacedFunction;
				expr->valueType = dotOperator->namespacedFunction->returnType->typeID;
				expr->lvalue = false;
				expr->isMethodCall = false;

				functionType = dotOperator->namespacedFunction->type;
				functionAst = dotOperator->namespacedFunction;
			}
		}
		else
		{
			result = false;
		}
	}

	if (!functionType)
	{
		if (ResolveExpression(resolver, expr->calleeExpr))
		{
			if (expr->calleeexpr->valueType->typeKind == TYPE_KIND_FUNCTION)
			{
				expr->valueType = expr->calleeexpr->valueType->functionType.returnType;
				expr->lvalue = false;

				functionType = expr->valueType;
				functionAst = NULL;
			}
			else
			{
				SnekError(resolver->context, expr->calleeExpr->location, ERROR_CODE_NON_INVOKABLE_EXPRESSION, "Can't invoke expression %.16s", expr->calleeExpr->location.ptr);
				result = false;
			}
		}
		else
		{
			result = false;
		}
	}
	*/

	if (result)
	{
		for (int i = 0; i < expr->genericArgs.size; i++)
		{
			// TODO check generic arg count

			if (ResolveType(resolver, expr->genericArgs[i]))
			{
			}
			else
			{
				result = false;
			}
		}

		if (result)
		{
			for (int i = 0; i < expr->arguments.size; i++)
			{
				int paramOffset = expr->isMethodCall ? 1 : 0;
				int paramCount = functionType->functionType.numParams - (expr->isMethodCall ? 1 : 0);

				if (i < paramCount)
				{
					TypeID argType = expr->arguments[i]->valueType;
					TypeID paramType = functionType->functionType.paramTypes[paramOffset + i];
					bool argIsConstant = expr->arguments[i]->isConstant();

					if (!CanConvertImplicit(argType, paramType, argIsConstant))
					{
						result = false;

						const char* argTypeStr = GetTypeString(argType);
						const char* paramTypeStr = GetTypeString(paramType);

						if (function)
						{
							SnekError(resolver->context, expr->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong type of argument #%d '%s': should be %s instead of %s", i, function->paramNames[paramOffset + i], paramTypeStr, argTypeStr);
							result = false;
						}
						else
						{
							SnekError(resolver->context, expr->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong type of argument #%d: should be %s instead of %s", i, paramTypeStr, argTypeStr);
							result = false;
						}
					}
				}
			}
		}
	}

	return result;
}

static bool ResolveSubscriptOperator(Resolver* resolver, AST::SubscriptOperator* expr)
{
	if (ResolveExpression(resolver, expr->operand))
	{
		bool resolved = true;
		for (int i = 0; i < expr->arguments.size; i++)
		{
			if (!ResolveExpression(resolver, expr->arguments[i]))
				resolved = false;
		}

		if (resolved)
		{
			if (expr->operand->valueType->typeKind == AST::TypeKind::Pointer)
			{
				if (expr->arguments.size == 1)
				{
					if (expr->operand->valueType->pointerType.elementType->typeKind == AST::TypeKind::Array)
					{
						expr->valueType = expr->operand->valueType->pointerType.elementType->arrayType.elementType;
					}
					else
					{
						expr->valueType = expr->operand->valueType->pointerType.elementType;
					}
					expr->lvalue = true;
					return true;
				}
				else
				{
					SnekAssert(false);
					return false;
				}
			}
			else if (expr->operand->valueType->typeKind == AST::TypeKind::Array)
			{
				if (expr->arguments.size == 1)
				{
					expr->valueType = expr->operand->valueType->arrayType.elementType;
					expr->lvalue = true;
					return true;
				}
				else
				{
					SnekAssert(false);
					return false;
				}
			}
			else if (expr->operand->valueType->typeKind == AST::TypeKind::String)
			{
				if (expr->arguments.size == 1)
				{
					expr->valueType = GetIntegerType(8, true);
					expr->lvalue = true;
					return true;
				}
				else
				{
					SnekAssert(false);
					return false;
				}
			}
			else
			{
				SnekAssert(false);
				return false;
			}
		}
	}
	return false;
}

/*
static AstModule* GetModuleFromNamespace(Resolver* resolver, const char* nameSpace)
{
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstModule* module = resolver->context->asts[i];
		if (module->nameSpace && strcmp(module->nameSpace, nameSpace) == 0)
			return module;
	}
	return NULL;
}
*/

static bool ResolveDotOperator(Resolver* resolver, AST::DotOperator* expr)
{
	if (expr->operand->type == AST::ExpressionType::Identifier)
	{
		const char* nameSpace = ((AST::Identifier*)expr->operand)->name;

		//if (AST::File* file = FindFileWithNamespace(resolver, nameSpace, resolver->currentFile))
		if (AST::Module* module = FindModuleInDependencies(resolver, nameSpace, resolver->currentFile))
		{
			//if (AST::File* file = module->file)
			for (AST::File* file : module->files)
			{
				if (resolver->findFunctionsInFile(expr->name, file, expr->namespacedFunctions))
				{
					//if (function->visibility >= AST::Visibility::Public || file->module == resolver->currentFile->module)
					//{
						//expr->namespacedFunction = function;
					expr->valueType = expr->namespacedFunctions[0]->functionType;
					expr->lvalue = false;
					return true;
					//}
					//else
					//{
					//	SnekError(resolver->context, expr->location, ERROR_CODE_NON_VISIBLE_DECLARATION, "Function '%s' is not visible", function->name);
					//	return false;
					//}
				}
				if (Variable* variable = resolver->findGlobalVariableInFile(expr->name, file))
				{
					expr->namespacedVariable = variable;
					expr->valueType = variable->type;
					expr->lvalue = true;
					return true;
				}
			}

			SnekError(resolver->context, expr->location, ERROR_CODE_UNRESOLVED_IDENTIFIER, "No identifier '%s' in file '%s'", expr->name, nameSpace);
			return false;
		}
		/*
		else
		{
			AstModule* ns = NULL;

			if (!ns) ns = FindModuleInDependencies(resolver, nameSpace, resolver->file); // Search in imported namespaces
			if (!ns) ns = FindModule(resolver, nameSpace, NULL); // Search in global namespace

			if (ns)
			{
				if (AstFunction* function = FindFunctionInNamespace(resolver, ns, expr->name, resolver->file->module))
				{
					expr->namespacedFunction = function;
					expr->valueType = function->type;
					expr->lvalue = false;
					return true;
				}
				if (AstVariable* variable = FindGlobalVariableInNamespace(resolver, ns, expr->name, resolver->file->module))
				{
					expr->namespacedVariable = variable;
					expr->valueType = variable->type;
					expr->lvalue = true;
					return true;
				}
				if (AstModule* nestedNs = FindModule(resolver, expr->name, ns))
				{
					expr->ns = nestedNs;
					expr->valueType = NULL;
					return true;
				}

				SnekError(resolver->context, expr->location, ERROR_CODE_UNRESOLVED_IDENTIFIER, "Unresolved identifier '%s' in module %s", expr->name, ns->name);
				return false;
			}
		}
		*/
	}
	if (ResolveExpression(resolver, expr->operand))
	{
		if (expr->operand->type == AST::ExpressionType::DotOperator)
		{
			if (AST::Module* module = ((AST::DotOperator*)expr->operand)->module)
			{
				//if (AST::File* file = module->file)
				for (AST::File* file : module->files)
				{
					if (resolver->findFunctionsInFile(expr->name, file, expr->namespacedFunctions))
					{
						//expr->namespacedFunction = function;
						expr->valueType = expr->namespacedFunctions[0]->functionType;
						expr->lvalue = false;
						return true;
					}
				}
				if (AST::Module* nestedNs = FindModule(resolver, expr->name, module))
				{
					expr->module = nestedNs;
					expr->valueType = NULL;
					return true;
				}

				SnekError(resolver->context, expr->location, ERROR_CODE_UNRESOLVED_IDENTIFIER, "Unresolved identifier '%s' in namespace %s", expr->name, module->name);
				return false;
			}
		}

		TypeID operandType = expr->operand->valueType;

		while (operandType->typeKind == AST::TypeKind::Alias)
			operandType = operandType->aliasType.alias;

		if (operandType->typeKind == AST::TypeKind::Pointer)
			operandType = operandType->pointerType.elementType;

		while (operandType->typeKind == AST::TypeKind::Alias)
			operandType = operandType->aliasType.alias;

		if (operandType->typeKind == AST::TypeKind::Struct)
		{
			for (int i = 0; i < operandType->structType.declaration->fields.size; i++)
			{
				AST::StructField* field = operandType->structType.declaration->fields[i];
				TypeID memberType = operandType->structType.fieldTypes[i];
				if (strcmp(field->name, expr->name) == 0)
				{
					expr->valueType = memberType;
					expr->lvalue = true;
					expr->structField = field;
					return true;
				}
			}

			if (AST::Function* method = resolver->findFunction(expr->name))
			{
				expr->valueType = method->functionType;
				expr->lvalue = false;
				expr->classMethod = method;
				return true;
			}

			SnekError(resolver->context, expr->location, ERROR_CODE_UNRESOLVED_MEMBER, "Unresolved struct member %s.%s", GetTypeString(expr->operand->valueType), expr->name);
			return false;
		}
		else if (operandType->typeKind == AST::TypeKind::Class)
		{
			for (int i = 0; i < operandType->classType.declaration->fields.size; i++)
			{
				AST::ClassField* field = operandType->classType.declaration->fields[i];
				TypeID memberType = operandType->classType.fieldTypes[i];
				if (strcmp(field->name, expr->name) == 0)
				{
					expr->valueType = memberType;
					expr->lvalue = true;
					expr->classField = field;
					return true;
				}
			}
			for (int i = 0; i < operandType->classType.declaration->methods.size; i++)
			{
				AST::Method* method = operandType->classType.declaration->methods[i];
				if (strcmp(method->name, expr->name) == 0)
				{
					expr->valueType = method->functionType;
					expr->lvalue = false;
					expr->classMethod = method;
					return true;
				}
			}

			SnekError(resolver->context, expr->location, ERROR_CODE_UNRESOLVED_MEMBER, "Unresolved class member %s.%s", GetTypeString(expr->operand->valueType), expr->name);
			return false;
		}
		else if (operandType->typeKind == AST::TypeKind::Array)
		{
			expr->arrayField = -1;
			if (strcmp(expr->name, "size") == 0)
			{
				expr->valueType = GetIntegerType(32, false);
				expr->lvalue = false;
				expr->arrayField = 0;
				return true;
			}
			else if (strcmp(expr->name, "ptr") == 0)
			{
				expr->valueType = GetPointerType(expr->operand->valueType->arrayType.elementType);
				expr->lvalue = false;
				expr->arrayField = 1;
				return true;
			}

			SnekError(resolver->context, expr->location, ERROR_CODE_UNRESOLVED_MEMBER, "Unresolved array property %s.%s", GetTypeString(expr->operand->valueType), expr->name);
			return false;
		}
		else if (operandType->typeKind == AST::TypeKind::String)
		{
			expr->stringField = -1;
			if (strcmp(expr->name, "length") == 0)
			{
				expr->valueType = GetIntegerType(32, false);
				expr->lvalue = false;
				expr->stringField = 0;
				return true;
			}
			else if (strcmp(expr->name, "ptr") == 0)
			{
				expr->valueType = GetPointerType(GetIntegerType(8, true));
				expr->lvalue = false;
				expr->stringField = 1;
				return true;
			}

			SnekError(resolver->context, expr->location, ERROR_CODE_UNRESOLVED_MEMBER, "Unresolved string property %s.%s", GetTypeString(expr->operand->valueType), expr->name);
			return false;
		}
		else
		{
			SnekError(resolver->context, expr->location, ERROR_CODE_UNRESOLVED_MEMBER, "Unresolved property %s.%s", GetTypeString(expr->operand->valueType), expr->name);
			return false;
		}
	}
	else
	{
		return false;
	}

	SnekAssert(false);
	return false;
}

static bool ResolveCast(Resolver* resolver, AST::Typecast* expr)
{
	if (ResolveType(resolver, expr->dstType))
	{
		if (ResolveExpression(resolver, expr->value))
		{
			if (CanConvert(expr->value->valueType, expr->dstType->typeID))
			{
				expr->valueType = expr->dstType->typeID;
				expr->lvalue = false;
				return true;
			}
			else
			{
				SnekError(resolver->context, expr->location, ERROR_CODE_INVALID_CAST, "Invalid cast: %s to %s", GetTypeString(expr->value->valueType), GetTypeString(expr->dstType->typeID));
			}
		}
	}
	return false;
}

static bool ResolveSizeof(Resolver* resolver, AST::Sizeof* expr)
{
	if (ResolveType(resolver, expr->dstType))
	{
		expr->valueType = GetIntegerType(64, false);
		expr->lvalue = false;
		return true;
	}
	return false;
}

static bool ResolveMalloc(Resolver* resolver, AST::Malloc* expr)
{
	bool result = true;

	if (ResolveType(resolver, expr->dstType))
	{
		if (expr->dstType->typeKind == AST::TypeKind::Array)
		{
			AST::ArrayType* arrayType = (AST::ArrayType*)expr->dstType;

			if (!expr->malloc && !arrayType->length->isConstant())
			{
				SnekError(resolver->context, arrayType->location, ERROR_CODE_ARRAY_LENGTH_WRONG_TYPE, "Dynamically sized arrays need to be allocated on the heap using 'new'");
				result = false;
			}
			if (!arrayType->length)
			{
				SnekError(resolver->context, expr->location, ERROR_CODE_MALLOC_INVALID_TYPE, "Allocating string needs a length");
				result = false;
			}

			if (expr->count)
			{
				SnekError(resolver->context, expr->location, ERROR_CODE_MALLOC_INVALID_TYPE, "Can't allocate multiple strings at once");
				result = false;
			}

			expr->valueType = GetPointerType(GetArrayType(arrayType->typeID->arrayType.elementType, arrayType->typeID->arrayType.length));
			expr->lvalue = false;
		}
		else if (expr->dstType->typeKind == AST::TypeKind::String)
		{
			AST::StringType* stringType = (AST::StringType*)expr->dstType;

			if (!expr->malloc && !stringType->length->isConstant())
			{
				SnekError(resolver->context, stringType->location, ERROR_CODE_ARRAY_LENGTH_WRONG_TYPE, "Stack allocated strings need to have a constant length");
				result = false;
			}
			if (!stringType->length)
			{
				SnekError(resolver->context, expr->location, ERROR_CODE_MALLOC_INVALID_TYPE, "Allocating string needs a length");
				result = false;
			}

			if (expr->count)
			{
				SnekError(resolver->context, expr->location, ERROR_CODE_MALLOC_INVALID_TYPE, "Can't allocate multiple strings at once");
				result = false;
			}

			expr->valueType = GetPointerType(GetStringType());
			expr->lvalue = false;
		}
		else if (expr->dstType->typeKind == AST::TypeKind::Class)
		{
			if (AST::Constructor* constructor = expr->dstType->typeID->classType.declaration->constructor)
			{
				if (expr->arguments.size < constructor->paramTypes.size || expr->arguments.size > constructor->paramTypes.size && !constructor->varArgs)
				{
					SnekError(resolver->context, expr->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments in constructor: should be %d instead of %d", constructor->paramTypes.size, expr->arguments.size);
					result = false;
				}
				for (int i = 0; i < expr->arguments.size; i++)
				{
					if (ResolveExpression(resolver, expr->arguments[i]))
					{
						if (i < constructor->paramTypes.size)
						{
							TypeID argType = expr->arguments[i]->valueType;
							TypeID paramType = constructor->paramTypes[i]->typeID;
							const char* argTypeStr = GetTypeString(expr->arguments[i]->valueType);
							const char* paramTypeStr = GetTypeString(constructor->paramTypes[i]->typeID);
							if (!CanConvertImplicit(argType, paramType, expr->arguments[i]->isConstant()))
							{
								SnekError(resolver->context, expr->arguments[i]->location, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong type of argument '%s': should be %s instead of %s", constructor->paramNames[i], paramTypeStr, argTypeStr);
								result = false;
							}
						}
					}
					else
					{
						result = false;
					}
				}
			}

			expr->valueType = expr->dstType->typeID;
			expr->lvalue = false;
		}
		else
		{
			if (expr->hasArguments)
			{
				SnekError(resolver->context, expr->dstType->location, ERROR_CODE_MALLOC_INVALID_TYPE, "Can't call constructor of non-class type");
				result = false;
			}
			else
			{
				expr->valueType = GetPointerType(expr->dstType->typeID);
				expr->lvalue = false;
			}
		}
		if (expr->count)
		{
			if (!ResolveExpression(resolver, expr->count))
				result = false;
			if (!expr->count->isConstant() && !expr->malloc)
			{
				SnekError(resolver->context, expr->count->location, ERROR_CODE_MALLOC_INVALID_TYPE, "Stack allocation instance count must be a constant integer");
				result = false;
			}
		}
	}
	else
	{
		result = false;
	}
	return result;
}

static bool ResolveUnaryOperator(Resolver* resolver, AST::UnaryOperator* expr)
{
	if (ResolveExpression(resolver, expr->operand))
	{
		if (!expr->position)
		{
			switch (expr->operatorType)
			{
			case AST::UnaryOperatorType::Not:
				expr->valueType = GetBoolType();
				expr->lvalue = false;
				return true;
			case AST::UnaryOperatorType::Negate:
				expr->valueType = expr->operand->valueType;
				expr->lvalue = false;
				return true;
			case AST::UnaryOperatorType::Reference:
				expr->valueType = GetPointerType(expr->operand->valueType);
				expr->lvalue = false;
				return true;
			case AST::UnaryOperatorType::Dereference:
				if (expr->operand->valueType->typeKind == AST::TypeKind::Pointer)
				{
					expr->valueType = expr->operand->valueType->pointerType.elementType;
					expr->lvalue = true;
					return true;
				}
				else
				{
					SnekError(resolver->context, expr->operand->location, ERROR_CODE_DEREFERENCE_INVALID_TYPE, "Can't dereference non-pointer type value");
					return false;
				}

			case AST::UnaryOperatorType::Increment:
				expr->valueType = expr->operand->valueType;
				expr->lvalue = true;
				return true;
			case AST::UnaryOperatorType::Decrement:
				expr->valueType = expr->operand->valueType;
				expr->lvalue = true;
				return true;

			default:
				SnekAssert(false);
				return false;
			}
		}
		else
		{
			switch (expr->operatorType)
			{
			case AST::UnaryOperatorType::Increment:
				expr->valueType = expr->operand->valueType;
				expr->lvalue = false;
				return true;
			case AST::UnaryOperatorType::Decrement:
				expr->valueType = expr->operand->valueType;
				expr->lvalue = false;
				return true;

			default:
				SnekAssert(false);
				return false;
			}
		}
	}
	return false;
}

static TypeID BinaryOperatorTypeMeet(Resolver* resolver, TypeID leftType, TypeID rightType)
{
	while (leftType->typeKind == AST::TypeKind::Alias)
		leftType = leftType->aliasType.alias;
	while (rightType->typeKind == AST::TypeKind::Alias)
		rightType = rightType->aliasType.alias;

	if (leftType->typeKind == AST::TypeKind::Integer && rightType->typeKind == AST::TypeKind::Integer)
	{
		int bitWidth = max(leftType->integerType.bitWidth, rightType->integerType.bitWidth);
		bool isSigned = leftType->integerType.isSigned || rightType->integerType.isSigned;
		return GetIntegerType(bitWidth, isSigned);
	}
	else if (leftType->typeKind == AST::TypeKind::FloatingPoint && rightType->typeKind == AST::TypeKind::FloatingPoint)
	{
		FloatingPointPrecision precision = (FloatingPointPrecision)max((int)leftType->fpType.precision, (int)rightType->fpType.precision);
		return GetFloatingPointType(precision);
	}
	else if (leftType->typeKind == AST::TypeKind::Integer && rightType->typeKind == AST::TypeKind::FloatingPoint)
	{
		return rightType;
	}
	else if (leftType->typeKind == AST::TypeKind::FloatingPoint && rightType->typeKind == AST::TypeKind::Integer)
	{
		return leftType;
	}
	else if (leftType->typeKind == AST::TypeKind::Pointer && rightType->typeKind == AST::TypeKind::Integer)
	{
		return leftType;
	}
	else if (leftType->typeKind == AST::TypeKind::Pointer && rightType->typeKind == AST::TypeKind::Pointer)
	{
		if (leftType->pointerType.elementType->typeKind == AST::TypeKind::Void)
			return rightType;
		if (rightType->pointerType.elementType->typeKind == AST::TypeKind::Void)
			return leftType;
		SnekAssert(false);
		return nullptr;
	}
	else if (leftType->typeKind == AST::TypeKind::String && rightType->typeKind == AST::TypeKind::String)
	{
		return leftType;
	}
	else
	{
		SnekAssert(false);
		return nullptr;
	}
}

static bool ResolveBinaryOperator(Resolver* resolver, AST::BinaryOperator* expr)
{
	bool result = true;

	result = ResolveExpression(resolver, expr->left) && result;
	result = ResolveExpression(resolver, expr->right) && result;

	if (result)
	{
		switch (expr->operatorType)
		{
		case AST::BinaryOperatorType::Add:
		case AST::BinaryOperatorType::Subtract:
		case AST::BinaryOperatorType::Multiply:
		case AST::BinaryOperatorType::Divide:
		case AST::BinaryOperatorType::Modulo:
			expr->valueType = BinaryOperatorTypeMeet(resolver, expr->left->valueType, expr->right->valueType);
			expr->lvalue = false;
			return true;

		case AST::BinaryOperatorType::Equals:
		case AST::BinaryOperatorType::DoesNotEqual:
		case AST::BinaryOperatorType::LessThan:
		case AST::BinaryOperatorType::GreaterThan:
		case AST::BinaryOperatorType::LessThanEquals:
		case AST::BinaryOperatorType::GreaterThanEquals:
			expr->valueType = GetBoolType();
			expr->lvalue = false;
			return true;

		case AST::BinaryOperatorType::LogicalAnd:
		case AST::BinaryOperatorType::LogicalOr:
			if (CanConvertImplicit(expr->left->valueType, GetBoolType(), false) && CanConvertImplicit(expr->right->valueType, GetBoolType(), false))
			{
				expr->valueType = GetBoolType();
				expr->lvalue = false;
				return true;
			}
			else
			{
				if (expr->operatorType == AST::BinaryOperatorType::LogicalAnd)
					SnekError(resolver->context, expr->left->location, ERROR_CODE_BIN_OP_INVALID_TYPE, "Operands of && must be of bool type");
				else if (expr->operatorType == AST::BinaryOperatorType::LogicalOr)
					SnekError(resolver->context, expr->left->location, ERROR_CODE_BIN_OP_INVALID_TYPE, "Operands of || must be of bool type");
				else
					SnekAssert(false);
				return false;
			}

		case AST::BinaryOperatorType::BitwiseAnd:
		case AST::BinaryOperatorType::BitwiseOr:
		case AST::BinaryOperatorType::BitwiseXor:
		case AST::BinaryOperatorType::BitshiftLeft:
		case AST::BinaryOperatorType::BitshiftRight:
			expr->valueType = BinaryOperatorTypeMeet(resolver, expr->left->valueType, expr->right->valueType);
			expr->lvalue = false;
			return true;

		case AST::BinaryOperatorType::Assignment:
		case AST::BinaryOperatorType::PlusEquals:
		case AST::BinaryOperatorType::MinusEquals:
		case AST::BinaryOperatorType::TimesEquals:
		case AST::BinaryOperatorType::DividedByEquals:
		case AST::BinaryOperatorType::ModuloEquals:
		case AST::BinaryOperatorType::BitshiftLeftEquals:
		case AST::BinaryOperatorType::BitshiftRightEquals:
		case AST::BinaryOperatorType::BitwiseAndEquals:
		case AST::BinaryOperatorType::BitwiseOrEquals:
		case AST::BinaryOperatorType::BitwiseXorEquals:
			if (expr->left->lvalue)
			{
				if (CanConvertImplicit(expr->right->valueType, expr->left->valueType, expr->right->isConstant()))
				{
					expr->valueType = expr->left->valueType;
					expr->lvalue = true;
					return true;
				}
				else
				{
					SnekError(resolver->context, expr->left->location, ERROR_CODE_BIN_OP_INVALID_TYPE, "Can't assign value of type '%s' to variable of type '%s'", GetTypeString(expr->right->valueType), GetTypeString(expr->left->valueType));
					return false;
				}
			}
			else
			{
				SnekError(resolver->context, expr->left->location, ERROR_CODE_BIN_OP_INVALID_TYPE, "Can't assign to a non lvalue");
				return false;
			}

		default:
			SnekAssert(false);
			return false;
		}
	}
	return false;
}

static bool ResolveTernaryOperator(Resolver* resolver, AST::TernaryOperator* expr)
{
	if (ResolveExpression(resolver, expr->condition) && ResolveExpression(resolver, expr->thenValue) && ResolveExpression(resolver, expr->elseValue))
	{
		TypeID type = BinaryOperatorTypeMeet(resolver, expr->thenValue->valueType, expr->elseValue->valueType);
		//if (CompareTypes(expr->thenValue->type, expr->elseValue->type))
		if (type)
		{
			expr->valueType = type;
			expr->lvalue = false;
			return true;
		}
		else
		{
			SnekError(resolver->context, expr->location, ERROR_CODE_TERNARY_OPERATOR_TYPE_MISMATCH, "Values of ternary operator must be of the same type: %s and %s", GetTypeString(expr->thenValue->valueType), GetTypeString(expr->elseValue->valueType));
			return false;
		}
	}
	return false;
}

static bool ResolveExpression(Resolver* resolver, AST::Expression* expression)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = expression;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	switch (expression->type)
	{
	case AST::ExpressionType::IntegerLiteral:
		return ResolveIntegerLiteral(resolver, (AST::IntegerLiteral*)expression);
	case AST::ExpressionType::FloatingPointLiteral:
		return ResolveFPLiteral(resolver, (AST::FloatingPointLiteral*)expression);
	case AST::ExpressionType::BooleanLiteral:
		return ResolveBoolLiteral(resolver, (AST::BooleanLiteral*)expression);
	case AST::ExpressionType::CharacterLiteral:
		return ResolveCharacterLiteral(resolver, (AST::CharacterLiteral*)expression);
	case AST::ExpressionType::NullLiteral:
		return ResolveNullLiteral(resolver, (AST::NullLiteral*)expression);
	case AST::ExpressionType::StringLiteral:
		return ResolveStringLiteral(resolver, (AST::StringLiteral*)expression);
	case AST::ExpressionType::InitializerList:
		return ResolveInitializerList(resolver, (AST::InitializerList*)expression);
	case AST::ExpressionType::Identifier:
		return ResolveIdentifier(resolver, (AST::Identifier*)expression);
	case AST::ExpressionType::Compound:
		return ResolveCompoundExpression(resolver, (AST::CompoundExpression*)expression);

	case AST::ExpressionType::FunctionCall:
		return ResolveFunctionCall(resolver, (AST::FunctionCall*)expression);
	case AST::ExpressionType::SubscriptOperator:
		return ResolveSubscriptOperator(resolver, (AST::SubscriptOperator*)expression);
	case AST::ExpressionType::DotOperator:
		return ResolveDotOperator(resolver, (AST::DotOperator*)expression);
	case AST::ExpressionType::Typecast:
		return ResolveCast(resolver, (AST::Typecast*)expression);
	case AST::ExpressionType::Sizeof:
		return ResolveSizeof(resolver, (AST::Sizeof*)expression);
	case AST::ExpressionType::Malloc:
		return ResolveMalloc(resolver, (AST::Malloc*)expression);

	case AST::ExpressionType::UnaryOperator:
		return ResolveUnaryOperator(resolver, (AST::UnaryOperator*)expression);
	case AST::ExpressionType::BinaryOperator:
		return ResolveBinaryOperator(resolver, (AST::BinaryOperator*)expression);
	case AST::ExpressionType::TernaryOperator:
		return ResolveTernaryOperator(resolver, (AST::TernaryOperator*)expression);

	default:
		SnekAssert(false);
		return false;
	}
}

static bool ResolveStatement(Resolver* resolver, AST::Statement* statement);

static bool ResolveCompoundStatement(Resolver* resolver, AST::CompoundStatement* statement)
{
	bool result = true;

	resolver->pushScope("");
	for (int i = 0; i < statement->statements.size; i++)
	{
		result = ResolveStatement(resolver, statement->statements[i]) && result;
	}
	resolver->popScope();

	return result;
}

static bool ResolveExprStatement(Resolver* resolver, AST::ExpressionStatement* statement)
{
	return ResolveExpression(resolver, statement->expression);
}

static bool ResolveVarDeclStatement(Resolver* resolver, AST::VariableDeclaration* statement)
{
	bool result = true;

	if (ResolveType(resolver, statement->varType))
	{
		for (int i = 0; i < statement->declarators.size; i++)
		{
			AST::VariableDeclarator* declarator = statement->declarators[i];

			// Check if variable with that name already exists in the current function
			if (Variable* variableWithSameName = resolver->findVariable(declarator->name))
			{
				AST::Element* declaration = variableWithSameName->declaration;
				SnekWarn(resolver->context, statement->location, ERROR_CODE_VARIABLE_SHADOWING, "Variable with name '%s' already exists in module %s at %d:%d", declarator->name, declaration->file->name, declaration->location.line, declaration->location.col);
			}

			if (declarator->value)
			{
				if (ResolveExpression(resolver, declarator->value))
				{
					if (declarator->value->type == AST::ExpressionType::InitializerList && statement->varType->typeKind == AST::TypeKind::Array)
					{
						AST::InitializerList* initializerList = (AST::InitializerList*)declarator->value;
						TypeID arrayType = statement->varType->typeID;
						initializerList->initializerType = arrayType;
						initializerList->valueType = arrayType;
					}
					else if (declarator->value->valueType && declarator->value->valueType->typeKind == AST::TypeKind::Pointer && CompareTypes(declarator->value->valueType->pointerType.elementType, statement->varType->typeID))
						;
					else if (!CanConvertImplicit(declarator->value->valueType, statement->varType->typeID, declarator->value->isConstant()))
					{
						SnekError(resolver->context, statement->location, ERROR_CODE_BIN_OP_INVALID_TYPE, "Can't assign value of type '%s' to variable of type '%s'", GetTypeString(declarator->value->valueType), GetTypeString(statement->varType->typeID));
						result = false;
						//SnekError(resolver->context, statement->location, ERROR_CODE_INVALID_CAST, "Can't assign ");
					}
				}
				else
				{
					result = false;
				}
			}

			Variable* variable = new Variable(declarator->file, declarator->name, statement->varType->typeID, declarator->value, statement->isConstant, AST::Visibility::Null);
			declarator->variable = variable;
			resolver->registerLocalVariable(variable, statement);
		}
	}
	else
	{
		result = false;
	}

	return result;
}

static bool ResolveIfStatement(Resolver* resolver, AST::IfStatement* statement)
{
	bool result = true;

	result = ResolveExpression(resolver, statement->condition) && result;
	result = ResolveStatement(resolver, statement->thenStatement) && result;
	if (statement->elseStatement)
	{
		result = ResolveStatement(resolver, statement->elseStatement) && result;
	}

	return result;
}

static bool ResolveWhileLoop(Resolver* resolver, AST::WhileLoop* statement)
{
	bool result = true;

	resolver->pushScope("");
	resolver->scope->branchDst = statement;

	result = ResolveExpression(resolver, statement->condition) && result;
	result = ResolveStatement(resolver, statement->body) && result;

	resolver->popScope();

	return result;
}

static bool ResolveForLoop(Resolver* resolver, AST::ForLoop* statement)
{
	bool result = true;

	resolver->pushScope("");
	resolver->scope->branchDst = statement;

	result = ResolveExpression(resolver, statement->startValue) && ResolveExpression(resolver, statement->endValue) && result;
	if (statement->deltaValue)
	{
		result = ResolveExpression(resolver, statement->deltaValue) && result;

		if (statement->deltaValue->isConstant())
		{
			bool constantFoldSuccess;
			statement->delta = (int)ConstantFoldInt(resolver, statement->deltaValue, constantFoldSuccess);
			//statement->delta = (int)((AST::IntegerLiteral*)statement->deltaValue)->value;
		}
		else
		{
			SnekError(resolver->context, statement->deltaValue->location, ERROR_CODE_FOR_LOOP_SYNTAX, "For loop step value must be a constant integer");
			result = false;
		}
	}
	else
	{
		statement->delta = 1;
	}

	//statement->iterator = RegisterLocalVariable(resolver, GetIntegerType(32, true), statement->startValue, statement->iteratorName, false, resolver->file, statement->location);
	Variable* iterator = new Variable(statement->file, statement->iteratorName->name, GetIntegerType(32, true), statement->startValue, false, AST::Visibility::Null);
	statement->iterator = iterator;
	resolver->registerLocalVariable(iterator, statement->iteratorName);

	result = ResolveStatement(resolver, statement->body) && result;

	resolver->popScope();

	return result;
}

static bool ResolveBreak(Resolver* resolver, AST::Break* statement)
{
	Scope* scope = resolver->scope;
	while (!scope->branchDst)
	{
		if (scope->parent)
			scope = scope->parent;
		else
			break;
	}
	if (scope->branchDst)
	{
		statement->branchDst = scope->branchDst;
		return true;
	}
	else
	{
		SnekError(resolver->context, statement->location, ERROR_CODE_INVALID_BREAK, "No loop structure to break out of");
		return false;
	}
}

static bool ResolveContinue(Resolver* resolver, AST::Continue* statement)
{
	Scope* scope = resolver->scope;
	while (!scope->branchDst)
	{
		if (scope->parent)
			scope = scope->parent;
		else
			break;
	}
	if (scope->branchDst)
	{
		statement->branchDst = scope->branchDst;
		return true;
	}
	else
	{
		SnekError(resolver->context, statement->location, ERROR_CODE_INVALID_CONTINUE, "No loop structure to continue");
		return false;
	}
}

static bool ResolveReturn(Resolver* resolver, AST::Return* statement)
{
	if (statement->value)
	{
		if (ResolveExpression(resolver, statement->value))
		{
			AST::Function* currentFunction = resolver->currentFunction;
			TypeID returnType = currentFunction->returnType ? currentFunction->returnType->typeID : GetVoidType();

			if (CanConvertImplicit(statement->value->valueType, returnType, statement->value->isLiteral()))
			{
				return true;
			}
			else
			{
				SnekError(resolver->context, statement->location, ERROR_CODE_RETURN_WRONG_TYPE, "Can't return value of type %s from function with return type %s", GetTypeString(statement->value->valueType), GetTypeString(returnType));
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}

static bool ResolveFree(Resolver* resolver, AST::Free* statement)
{
	bool result = true;

	for (int i = 0; i < statement->values.size; i++)
	{
		result = ResolveExpression(resolver, statement->values[i]) && result;
	}

	return result;
}

static bool ResolveStatement(Resolver* resolver, AST::Statement* statement)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = statement;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	switch (statement->type)
	{
	case AST::StatementType::NoOp:
		return true;
	case AST::StatementType::Compound:
		return ResolveCompoundStatement(resolver, (AST::CompoundStatement*)statement);
	case AST::StatementType::Expression:
		return ResolveExprStatement(resolver, (AST::ExpressionStatement*)statement);
	case AST::StatementType::VariableDeclaration:
		return ResolveVarDeclStatement(resolver, (AST::VariableDeclaration*)statement);
	case AST::StatementType::If:
		return ResolveIfStatement(resolver, (AST::IfStatement*)statement);
	case AST::StatementType::While:
		return ResolveWhileLoop(resolver, (AST::WhileLoop*)statement);
	case AST::StatementType::For:
		return ResolveForLoop(resolver, (AST::ForLoop*)statement);
	case AST::StatementType::Break:
		return ResolveBreak(resolver, (AST::Break*)statement);
	case AST::StatementType::Continue:
		return ResolveContinue(resolver, (AST::Continue*)statement);
	case AST::StatementType::Return:
		return ResolveReturn(resolver, (AST::Return*)statement);
	case AST::StatementType::Free:
		return ResolveFree(resolver, (AST::Free*)statement);

	default:
		SnekAssert(false);
		return false;
	}
}

static AST::Visibility GetVisibilityFromFlags(AST::DeclarationFlags flags)
{
	if (HasFlag(flags, AST::DeclarationFlags::Public))
		return AST::Visibility::Public;
	else if (HasFlag(flags, AST::DeclarationFlags::Private))
		return AST::Visibility::Private;
	else
		return AST::Visibility::Private;
}

static bool CheckEntrypointDecl(Resolver* resolver, AST::Function* function)
{
	if (function->visibility != AST::Visibility::Public)
	{
		SnekError(resolver->context, function->location, ERROR_CODE_ENTRY_POINT_DECLARATION, "Entry point must be public");
		return false;
	}
	if (!(!function->returnType || function->returnType->typeKind == AST::TypeKind::Void || function->returnType->typeKind == AST::TypeKind::Integer && function->returnType->typeID->integerType.bitWidth == 32 && function->returnType->typeID->integerType.isSigned))
	{
		SnekError(resolver->context, function->location, ERROR_CODE_ENTRY_POINT_DECLARATION, "Entry point must return void");
		return false;
	}
	if (function->paramTypes.size > 0)
	{
		SnekError(resolver->context, function->location, ERROR_CODE_ENTRY_POINT_DECLARATION, "Entry point must not have parameters");
		return false;
	}
	return true;
}

static bool ResolveFunctionHeader(Resolver* resolver, AST::Function* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	AST::Function* lastFunction = resolver->currentFunction;
	resolver->currentFunction = decl;

	bool result = true;

	decl->visibility = GetVisibilityFromFlags(decl->flags);

	decl->isEntryPoint = strcmp(decl->name, ENTRY_POINT_NAME) == 0;

	if (decl->isGeneric)
	{
	}
	else
	{
		if (decl->returnType)
			result = ResolveType(resolver, decl->returnType) && result;

		bool defaultArgumentExists = false;
		for (int i = 0; i < decl->paramTypes.size; i++)
		{
			result = ResolveType(resolver, decl->paramTypes[i]) && result;

			if (AST::Expression* paramValue = decl->paramValues[i])
			{
				result = result && ResolveExpression(resolver, paramValue);

				defaultArgumentExists = true;

				if (!paramValue->isConstant())
				{
					SnekError(resolver->context, paramValue->location, ERROR_CODE_FUNCTION_SYNTAX, "Default value for parameter '%s' must be a constant", decl->paramNames[i]);
					result = false;
				}
			}
			else
			{
				if (defaultArgumentExists)
				{
					SnekError(resolver->context, decl->paramTypes[i]->location, ERROR_CODE_FUNCTION_SYNTAX, "Parameter '%s' is following a parameter with a default argument and therefore must also have a default argument");
					result = false;
				}
			}
		}

		int numParams = decl->paramTypes.size;
		TypeID* paramTypes = new TypeID[numParams];
		for (int i = 0; i < numParams; i++)
		{
			paramTypes[i] = decl->paramTypes[i]->typeID;
		}
		decl->functionType = GetFunctionType(decl->returnType ? decl->returnType->typeID : GetVoidType(), numParams, paramTypes, decl->varArgs, false, nullptr, decl);

		if (decl->isEntryPoint)
		{
			result = CheckEntrypointDecl(resolver, decl) && result;
		}

		decl->mangledName = MangleFunctionName(decl);
	}

	resolver->currentFunction = lastFunction;

	return result;
}

static bool ResolveFunction(Resolver* resolver, AST::Function* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	if (HasFlag(decl->flags, AST::DeclarationFlags::Extern))
	{
		if (decl->body)
		{
			SnekError(resolver->context, decl->location, ERROR_CODE_FUNCTION_SYNTAX, "Extern function '%s' cannot have a body", decl->name);
			return false;
		}
	}

	if (decl->isGeneric)
	{
	}
	else
	{
		if (decl->body)
		{
			AST::Function* lastFunction = resolver->currentFunction;
			resolver->currentFunction = decl;

			resolver->pushScope(decl->name);

			decl->paramVariables;
			decl->paramVariables.resize(decl->paramTypes.size);

			for (int i = 0; i < decl->paramTypes.size; i++)
			{
				Variable* variable = new Variable(decl->file, decl->paramNames[i], decl->paramTypes[i]->typeID, nullptr, false, AST::Visibility::Null);
				decl->paramVariables[i] = variable;
				resolver->registerLocalVariable(variable, decl->paramTypes[i]);
				//RegisterLocalVariable(resolver, decl->paramTypes[i]->typeID, NULL, decl->paramNames[i], false, resolver->file, decl->paramTypes[i]->location);
			}

			result = ResolveStatement(resolver, decl->body) && result;

			resolver->popScope();

			resolver->currentFunction = lastFunction;
		}
	}

	return result;
}

static bool ResolveStructHeader(Resolver* resolver, AST::Struct* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	decl->visibility = GetVisibilityFromFlags(decl->flags);

	if (decl->isGeneric)
	{
	}
	else
	{
		decl->mangledName = MangleStructName(decl);
		decl->type = GetStructType(decl->name, decl);
	}

	return true;
}

static bool ResolveStruct(Resolver* resolver, AST::Struct* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	if (decl->isGeneric)
	{
	}
	else
	{
		decl->type->structType.hasBody = decl->hasBody;

		if (decl->hasBody)
		{
			AST::Struct* lastStruct = resolver->currentStruct;
			resolver->currentStruct = decl;

			int numFields = decl->fields.size;
			TypeID* fieldTypes = new TypeID[numFields];
			const char** fieldNames = new const char* [numFields];
			for (int i = 0; i < numFields; i++)
			{
				AST::StructField* field = decl->fields[i];
				if (ResolveType(resolver, field->type))
				{
					fieldTypes[i] = decl->fields[i]->type->typeID;
				}
				else
				{
					result = false;
				}
				fieldNames[i] = decl->fields[i]->name;
			}
			decl->type->structType.numFields = numFields;
			decl->type->structType.fieldTypes = fieldTypes;
			decl->type->structType.fieldNames = fieldNames;

			resolver->currentStruct = lastStruct;
		}
		else
		{
			decl->type->structType.numFields = 0;
			decl->type->structType.fieldTypes = NULL;
			decl->type->structType.fieldNames = NULL;
		}
	}

	return result;
}

static bool ResolveClassHeader(Resolver* resolver, AST::Class* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	decl->mangledName = _strdup(decl->name);
	decl->visibility = GetVisibilityFromFlags(decl->flags);
	decl->type = GetClassType(decl->name, decl);

	return true;
}

static bool ResolveClassMethodHeader(Resolver* resolver, AST::Method* method, AST::Class* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	method->mangledName = _strdup(method->name);

	result = ResolveType(resolver, method->returnType) && result;
	for (int i = 0; i < method->paramTypes.size; i++)
	{
		result = ResolveType(resolver, method->paramTypes[i]) && result;
	}

	int numParams = method->paramTypes.size;
	TypeID returnType = method->returnType->typeID;
	TypeID* paramTypes = new TypeID[numParams];
	for (int i = 0; i < numParams; i++)
	{
		paramTypes[i] = method->paramTypes[i]->typeID;
	}

	method->instanceType = decl->type;
	method->functionType = GetFunctionType(returnType, numParams, paramTypes, method->varArgs, true, decl->type, method);

	return result;
}

static bool ResolveClassConstructorHeader(Resolver* resolver, AST::Constructor* constructor, AST::Class* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	constructor->name = _strdup(decl->type->classType.name);
	constructor->mangledName = _strdup(constructor->name);

	bool result = true;

	result = ResolveType(resolver, constructor->returnType) && result;
	for (int i = 0; i < constructor->paramTypes.size; i++)
	{
		result = ResolveType(resolver, constructor->paramTypes[i]) && result;
	}

	int numParams = constructor->paramTypes.size;
	TypeID returnType = constructor->returnType->typeID;
	TypeID* paramTypes = new TypeID[numParams];
	for (int i = 0; i < numParams; i++)
	{
		paramTypes[i] = constructor->paramTypes[i]->typeID;
	}

	constructor->instanceType = decl->type;
	constructor->functionType = GetFunctionType(returnType, numParams, paramTypes, constructor->varArgs, true, decl->type, constructor);

	return result;
}

static bool ResolveClassProcedureHeaders(Resolver* resolver, AST::Class* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	int numMethods = decl->methods.size;
	for (int i = 0; i < numMethods; i++)
	{
		AST::Method* method = decl->methods[i];
		result = ResolveClassMethodHeader(resolver, method, decl) && result;
	}

	if (decl->constructor)
	{
		result = ResolveClassConstructorHeader(resolver, decl->constructor, decl) && result;
	}

	return result;
}

static bool ResolveClass(Resolver* resolver, AST::Class* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	int numFields = decl->fields.size;
	TypeID* fieldTypes = new TypeID[numFields];
	const char** fieldNames = new const char* [numFields];
	for (int i = 0; i < numFields; i++)
	{
		AST::ClassField* field = decl->fields[i];
		if (ResolveType(resolver, field->type))
		{
			fieldTypes[i] = decl->fields[i]->type->typeID;
		}
		else
		{
			result = false;
		}
		fieldNames[i] = decl->fields[i]->name;
	}
	decl->type->classType.numFields = numFields;
	decl->type->classType.fieldTypes = fieldTypes;
	decl->type->classType.fieldNames = fieldNames;

	return result;
}

static bool ResolveClassMethod(Resolver* resolver, AST::Function* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	AST::Function* lastFunction = resolver->currentFunction;
	resolver->currentFunction = decl;

	resolver->pushScope(decl->name);

	decl->paramVariables;
	decl->paramVariables.resize(decl->paramTypes.size);

	//decl->instanceVariable = RegisterLocalVariable(resolver, decl->instanceType, NULL, "this", false, resolver->file, decl->location);
	Variable* instanceVariable = new Variable(decl->file, _strdup("this"), decl->returnType->typeID, nullptr, false, AST::Visibility::Null);
	decl->instanceVariable = instanceVariable;
	resolver->registerLocalVariable(instanceVariable, decl);

	for (int i = 0; i < decl->paramTypes.size; i++)
	{
		//Variable* variable = RegisterLocalVariable(resolver, decl->paramTypes[i]->typeID, NULL, decl->paramNames[i], false, resolver->file, decl->paramTypes[i]->location);
		Variable* variable = new Variable(decl->file, _strdup(decl->paramNames[i]), decl->paramTypes[i]->typeID, nullptr, false, AST::Visibility::Null);
		decl->paramVariables[i] = variable;
		resolver->registerLocalVariable(variable, decl->paramTypes[i]);
	}

	result = ResolveStatement(resolver, decl->body) && result;

	resolver->popScope();

	resolver->currentFunction = lastFunction;

	return result;
}

static bool ResolveClassConstructor(Resolver* resolver, AST::Constructor* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	AST::Function* lastFunction = resolver->currentFunction;
	resolver->currentFunction = decl;

	resolver->pushScope(decl->name);

	decl->paramVariables;
	decl->paramVariables.resize(decl->paramTypes.size);

	//decl->instanceVariable = RegisterLocalVariable(resolver, decl->instanceType, NULL, "this", false, resolver->file, decl->location);
	Variable* instanceVariable = new Variable(decl->file, _strdup("this"), decl->instanceType, nullptr, false, AST::Visibility::Null);
	decl->instanceVariable = instanceVariable;
	resolver->registerLocalVariable(instanceVariable, decl);
	for (int i = 0; i < decl->paramTypes.size; i++)
	{
		//Variable* variable = RegisterLocalVariable(resolver, decl->paramTypes[i]->typeID, NULL, decl->paramNames[i], false, resolver->file, decl->location);
		Variable* variable = new Variable(decl->file, _strdup(decl->paramNames[i]), decl->paramTypes[i]->typeID, nullptr, false, AST::Visibility::Null);
		decl->paramVariables[i] = variable;
		resolver->registerLocalVariable(variable, decl->paramTypes[i]);
	}

	result = ResolveStatement(resolver, decl->body) && result;

	resolver->popScope();

	resolver->currentFunction = lastFunction;

	return result;
}

static bool ResolveClassProcedures(Resolver* resolver, AST::Class* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	int numMethods = decl->methods.size;
	for (int i = 0; i < numMethods; i++)
	{
		AST::Function* method = decl->methods[i];
		result = ResolveClassMethod(resolver, method) && result;
	}

	if (decl->constructor)
	{
		result = ResolveClassConstructor(resolver, decl->constructor) && result;
	}

	return result;
}

static bool ResolveTypedefHeader(Resolver* resolver, AST::Typedef* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	decl->visibility = GetVisibilityFromFlags(decl->flags);
	decl->type = GetAliasType(decl->name, decl);

	return true;
}

static bool ResolveTypedef(Resolver* resolver, AST::Typedef* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	result = ResolveType(resolver, decl->alias) && result;
	decl->type->aliasType.alias = decl->alias->typeID;

	return result;
}

static bool ResolveEnumHeader(Resolver* resolver, AST::Enum* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	decl->visibility = GetVisibilityFromFlags(decl->flags);
	decl->type = GetAliasType(decl->name, decl);
	decl->type->aliasType.alias = GetIntegerType(32, true);

	bool result = true;

	for (int i = 0; i < decl->values.size; i++)
	{
		decl->values[i]->declaration = decl;
		if (decl->values[i]->value)
		{
			if (!ResolveExpression(resolver, decl->values[i]->value))
			{
				result = false;
			}
		}
	}

	return true;
}

static bool ResolveEnum(Resolver* resolver, AST::Enum* decl)
{
	return true;
}

static bool ResolveExprdefHeader(Resolver* resolver, AST::Exprdef* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	decl->visibility = GetVisibilityFromFlags(decl->flags);

	return true;
}

static bool ResolveGlobalHeader(Resolver* resolver, AST::GlobalVariable* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	result = ResolveType(resolver, decl->type) && result;

	AST::Visibility visibility = GetVisibilityFromFlags(decl->flags);
	decl->visibility = visibility;
	for (int i = 0; i < decl->declarators.size; i++)
	{
		AST::VariableDeclarator* declarator = decl->declarators[i];
		//decl->variable = RegisterGlobalVariable(resolver, decl->type->typeID, decl->value, decl->name, decl->flags & DECL_FLAG_CONSTANT, visibility, resolver->file, decl);
		Variable* variable = new Variable(decl->file, _strdup(declarator->name), decl->type->typeID, declarator->value, HasFlag(decl->flags, AST::DeclarationFlags::Constant), visibility);
		resolver->registerGlobalVariable(variable, decl);
		declarator->variable = variable;
	}

	return result;
}

static bool ResolveGlobal(Resolver* resolver, AST::GlobalVariable* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	for (int i = 0; i < decl->declarators.size; i++)
	{
		AST::VariableDeclarator* declarator = decl->declarators[i];
		if (declarator->value)
		{
			if (ResolveExpression(resolver, declarator->value))
			{
				if (!CanConvertImplicit(declarator->value->valueType, decl->type->typeID, declarator->value->isConstant()))
				{
					SnekError(resolver->context, decl->location, ERROR_CODE_GLOBAL_TYPE_MISMATCH, "Can't initialize global variable '%s' of type %s with value of type %s", declarator->name, GetTypeString(decl->type->typeID), GetTypeString(declarator->value->valueType));
					result = false;
				}
				if (HasFlag(decl->flags, AST::DeclarationFlags::Extern))
				{
					SnekError(resolver->context, decl->location, ERROR_CODE_GLOBAL_EXTERN_INITIALIZER, "Extern global variable '%s' cannot have an initializer", declarator->name);
					result = false;
				}
				else if (!declarator->value->isConstant())
				{
					SnekError(resolver->context, decl->location, ERROR_CODE_GLOBAL_INITIALIZER_NONCONSTANT, "Initializer of global variable '%s' must be a constant value", declarator->name);
					result = false;
				}
			}
			else
			{
				result = false;
			}
		}
	}

	return result;
}

static bool ResolveModuleDecl(Resolver* resolver, AST::ModuleDeclaration* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	AST::Module* parent = resolver->globalNamespace;
	for (int i = 0; i < decl->identifier.namespaces.size; i++)
	{
		const char* name = decl->identifier.namespaces[i];
		AST::Module* module = FindModule(resolver, name, parent);
		if (!module)
		{
			module = CreateModule(resolver, name, parent);
		}
		parent = module;
	}

	//decl->module = CreateModule(resolver, decl->file->name, parent);
	decl->module = parent;

	return true;
}

static void AddModuleDependency(AST::File* file, AST::Module* module, bool recursive)
{
	file->dependencies.add(module);
	if (recursive)
	{
		for (int i = 0; i < module->children.size; i++)
		{
			AddModuleDependency(file, module->children[i], recursive);
		}
	}
}

static bool ResolveImport(Resolver* resolver, AST::Import* decl)
{
	AST::Element* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	for (int i = 0; i < decl->imports.size; i++)
	{
		AST::ModuleIdentifier& identifier = decl->imports[i];
		AST::Module* parent = resolver->globalNamespace;
		for (int j = 0; j < identifier.namespaces.size; j++)
		{
			const char* name = identifier.namespaces[j];
			if (strcmp(name, "*") == 0)
			{
				if (j == identifier.namespaces.size - 1)
				{
					for (int k = 0; k < parent->children.size; k++)
					{
						AddModuleDependency(resolver->currentFile, parent->children[k], false);
					}
					break;
				}
				else
				{
					SnekError(resolver->context, decl->location, ERROR_CODE_IMPORT_SYNTAX, "For bulk imports, * must be at the end of the import declaration");
					result = false;
					break;
				}
			}
			else if (strcmp(name, "**") == 0)
			{
				if (j == identifier.namespaces.size - 1)
				{
					AddModuleDependency(resolver->currentFile, parent, true);
					break;
				}
				else
				{
					SnekError(resolver->context, decl->location, ERROR_CODE_IMPORT_SYNTAX, "For recursive bulk imports, ** must be at the end of the import declaration");
					result = false;
					break;
				}
			}
			else
			{
				AST::Module* module = nullptr;

				if (!module && parent == resolver->globalNamespace)
				{
					module = FindModule(resolver, name, resolver->globalNamespace);
				}
				if (!module)
				{
					module = FindModule(resolver, name, parent);
				}

				if (module)
				{
					if (j == identifier.namespaces.size - 1)
					{
						AddModuleDependency(resolver->currentFile, module, false);
						break;
					}
					else
					{
						parent = module;
					}
				}
				else
				{
					char module_name[64];
					module_name[0] = '\0';
					for (int k = 0; k <= j; k++)
					{
						strcat(module_name, identifier.namespaces[k]);
						if (k < j)
							strcat(module_name, ".");
					}

					SnekError(resolver->context, decl->location, ERROR_CODE_UNKNOWN_MODULE, "Unknown module '%s'", module_name);
					result = false;
					parent = NULL;
					break;
				}
			}
		}
	}

	return result;
}

static bool ResolveModuleHeaders(Resolver* resolver)
{
	bool result = true;

	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;

		if (ast->moduleDecl)
		{
			result = ResolveModuleDecl(resolver, ast->moduleDecl) && result;
			ast->module = ast->moduleDecl->module;
		}
		else
		{
			//ast->module = CreateModule(resolver, ast->name, resolver->globalNamespace);
			ast->module = resolver->globalNamespace;
		}
		if (ast->namespaceDecl)
		{
			//result = ResolveNamespaceDecl(resolver, ast->namespaceDecl) && result;
			ast->nameSpace = ast->namespaceDecl->name;
		}

		ast->module->files.add(ast);
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->imports.size; j++)
		{
			result = ResolveImport(resolver, ast->imports[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->globals.size; j++)
		{
			bool isPrimitiveConstant = HasFlag(ast->globals[j]->flags, AST::DeclarationFlags::Constant) && IsPrimitiveType(ast->globals[j]->type->typeKind);
			if (isPrimitiveConstant)
				result = ResolveGlobalHeader(resolver, ast->globals[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->enums.size; j++)
		{
			result = ResolveEnumHeader(resolver, ast->enums[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->structs.size; j++)
		{
			result = ResolveStructHeader(resolver, ast->structs[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->classes.size; j++)
		{
			result = ResolveClassHeader(resolver, ast->classes[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->typedefs.size; j++)
		{
			result = ResolveTypedefHeader(resolver, ast->typedefs[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->exprdefs.size; j++)
		{
			result = ResolveExprdefHeader(resolver, ast->exprdefs[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->functions.size; j++)
		{
			result = ResolveFunctionHeader(resolver, ast->functions[j]) && result;
		}
		for (int j = 0; j < ast->classes.size; j++)
		{
			result = ResolveClassProcedureHeaders(resolver, ast->classes[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->globals.size; j++)
		{
			bool isPrimitiveConstant = HasFlag(ast->globals[j]->flags, AST::DeclarationFlags::Constant) && IsPrimitiveType(ast->globals[j]->type->typeKind);
			if (!isPrimitiveConstant)
				result = ResolveGlobalHeader(resolver, ast->globals[j]) && result;
		}
	}

	return result;
}

static bool ResolveModules(Resolver* resolver)
{
	bool result = true;

	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->globals.size; j++)
		{
			bool isPrimitiveConstant = HasFlag(ast->globals[j]->flags, AST::DeclarationFlags::Constant) && IsPrimitiveType(ast->globals[j]->type->typeKind);
			if (isPrimitiveConstant)
				result = ResolveGlobal(resolver, ast->globals[j]) && result;
		}
	}

	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->enums.size; j++)
		{
			result = ResolveEnum(resolver, ast->enums[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->structs.size; j++)
		{
			result = ResolveStruct(resolver, ast->structs[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->classes.size; j++)
		{
			result = ResolveClass(resolver, ast->classes[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->typedefs.size; j++)
		{
			result = ResolveTypedef(resolver, ast->typedefs[j]) && result;
		}
	}
	if (!result) // Return early if types have not been resolved
		return result;

	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->functions.size; j++)
		{
			result = ResolveFunction(resolver, ast->functions[j]) && result;
		}
		for (int j = 0; j < ast->classes.size; j++)
		{
			result = ResolveClassProcedures(resolver, ast->classes[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AST::File* ast = resolver->context->asts[i];
		resolver->currentFile = ast;
		for (int j = 0; j < ast->globals.size; j++)
		{
			bool isPrimitiveConstant = HasFlag(ast->globals[j]->flags, AST::DeclarationFlags::Constant) && IsPrimitiveType(ast->globals[j]->type->typeKind);
			if (!isPrimitiveConstant)
				result = ResolveGlobal(resolver, ast->globals[j]) && result;
		}
	}

	return result;
}

Resolver::Resolver(SkContext* context)
	: context(context)
{
	globalNamespace = CreateModule(this, "", nullptr);
}

Resolver::~Resolver()
{
	delete globalNamespace;
}

bool Resolver::run()
{
	bool result = true;

	InitTypeData();

	currentFile = nullptr;

	currentFunction = nullptr;
	currentElement = nullptr;

	scope = nullptr;

	result = ResolveModuleHeaders(this) && result;
	if (result) result = ResolveModules(this) && result;

	return result;
}

Scope* Resolver::pushScope(const char* name)
{
	Scope* newScope = new Scope();
	newScope->parent = scope;
	newScope->name = name;
	scope = newScope;
	return newScope;
}

void Resolver::popScope()
{
	Scope* oldScope = scope;
	scope = oldScope->parent;
	delete oldScope;
}

AST::File* Resolver::findFileByName(const char* name)
{
	for (int i = 0; i < context->asts.size; i++)
	{
		AST::File* file = context->asts[i];
		if (strcmp(file->name, name) == 0)
			return file;
	}
	return nullptr;
}

bool Resolver::isFunctionVisible(const AST::Function* function, AST::Module* currentModule)
{
	return function->file->module == currentModule || function->visibility >= AST::Visibility::Public;
}
