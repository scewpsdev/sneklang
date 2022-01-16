#include "Resolver.h"

#include "snek.h"
#include "variable.h"
#include "type.h"
#include "mangle.h"
#include "utils.h"

#include <math.h>

#include <stddef.h>


Resolver::Resolver(SkContext* context)
{
}

Resolver::~Resolver()
{
}

void Resolver::run()
{
}

void Resolver::pushScope(const char* name)
{
}

void Resolver::popScope()
{
}


static AstModule* CreateModule(Resolver* resolver, const char* name, AstModule* parent);

Resolver* CreateResolver(SkContext* context)
{
	Resolver* resolver = new Resolver;

	resolver->context = context;
	resolver->globalNamespace = CreateModule(resolver, "", NULL);

	return resolver;
}

void DestroyResolver(Resolver* resolver)
{
	delete resolver;
}

static AstModule* CreateModule(Resolver* resolver, const char* name, AstModule* parent)
{
	AstModule* ns = new AstModule;
	ns->name = _strdup(name);
	ns->parent = parent;
	ns->children = CreateList<AstModule*>();
	ns->files = CreateList<AstFile*>();

	if (parent)
		parent->children.add(ns);

	return ns;
}

static AstModule* FindModule(Resolver* resolver, const char* name, AstModule* parent)
{
	if (!parent)
		parent = resolver->globalNamespace;
	for (int i = 0; i < parent->children.size; i++)
	{
		if (strcmp(name, parent->children[i]->name) == 0)
			return parent->children[i];
	}
	return NULL;
}

static AstModule* FindModuleInDependencies(Resolver* resolver, const char* name, AstFile* file)
{
	for (int i = 0; i < file->dependencies.size; i++)
	{
		if (AstModule* module = FindModule(resolver, name, file->dependencies[i]))
		{
			return module;
		}
	}
	return NULL;
}

static AstFile* FindFileWithNamespace(Resolver* resolver, const char* name, AstFile* parent)
{
	for (int i = 0; i < parent->dependencies.size; i++)
	{
		AstModule* dependency = parent->dependencies[i];
		for (int j = 0; j < dependency->files.size; j++)
		{
			AstFile* file = dependency->files[j];
			if (file->nameSpace && strcmp(file->nameSpace, name) == 0)
				return file;
		}
	}
	return NULL;
}

static bool ResolveType(Resolver* resolver, AstType* type);
static bool ResolveExpression(Resolver* resolver, AstExpression* expr);

static bool IsPrimitiveType(AstTypeKind typeKind)
{
	return typeKind == TYPE_KIND_INTEGER
		|| typeKind == TYPE_KIND_FP
		|| typeKind == TYPE_KIND_BOOL;
}

static int64_t ConstantFoldInt(Resolver* resolver, AstExpression* expr, bool& success)
{
	switch (expr->exprKind)
	{
	case EXPR_KIND_INTEGER_LITERAL:
		return ((AstIntegerLiteral*)expr)->value;
	case EXPR_KIND_IDENTIFIER:
	{
		AstVariable* variable = ((AstIdentifier*)expr)->variable;
		if (!variable)
		{
			SnekError(resolver->context, expr->inputState, ERROR_CODE_CONSTANT_INITIALIZER_NON_CONSTANT, "Variable '%s' must be defined before using it to initialize a constant", ((AstIdentifier*)expr)->name);
			success = false;
			return 0;
		}
		else
		{
			SnekAssert(variable->isConstant);
			return ConstantFoldInt(resolver, variable->value, success);
		}
	}
	case EXPR_KIND_COMPOUND:
		return ConstantFoldInt(resolver, ((AstCompoundExpression*)expr)->value, success);

	case EXPR_KIND_UNARY_OPERATOR:
	{
		AstUnaryOperator* unaryOperator = (AstUnaryOperator*)expr;
		switch (unaryOperator->operatorType)
		{
		case UNARY_OPERATOR_NEGATE:
			return -ConstantFoldInt(resolver, unaryOperator->operand, success);

		default:
			SnekAssert(false);
			return 0;
		}
	}
	case EXPR_KIND_BINARY_OPERATOR:
	{
		AstBinaryOperator* binaryOperator = (AstBinaryOperator*)expr;
		switch (binaryOperator->operatorType)
		{
		case BINARY_OPERATOR_ADD:
			return ConstantFoldInt(resolver, binaryOperator->left, success) + ConstantFoldInt(resolver, binaryOperator->right, success);
		case BINARY_OPERATOR_SUB:
			return ConstantFoldInt(resolver, binaryOperator->left, success) - ConstantFoldInt(resolver, binaryOperator->right, success);
		case BINARY_OPERATOR_MUL:
			return ConstantFoldInt(resolver, binaryOperator->left, success) * ConstantFoldInt(resolver, binaryOperator->right, success);
		case BINARY_OPERATOR_DIV:
			return ConstantFoldInt(resolver, binaryOperator->left, success) / ConstantFoldInt(resolver, binaryOperator->right, success);
		case BINARY_OPERATOR_MOD:
			return ConstantFoldInt(resolver, binaryOperator->left, success) % ConstantFoldInt(resolver, binaryOperator->right, success);

		case BINARY_OPERATOR_BITWISE_AND:
			return ConstantFoldInt(resolver, binaryOperator->left, success) & ConstantFoldInt(resolver, binaryOperator->right, success);
		case BINARY_OPERATOR_BITWISE_OR:
			return ConstantFoldInt(resolver, binaryOperator->left, success) | ConstantFoldInt(resolver, binaryOperator->right, success);
		case BINARY_OPERATOR_BITWISE_XOR:
			return ConstantFoldInt(resolver, binaryOperator->left, success) ^ ConstantFoldInt(resolver, binaryOperator->right, success);
		case BINARY_OPERATOR_BITSHIFT_LEFT:
			return ConstantFoldInt(resolver, binaryOperator->left, success) << ConstantFoldInt(resolver, binaryOperator->right, success);
		case BINARY_OPERATOR_BITSHIFT_RIGHT:
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

static bool ResolveVoidType(Resolver* resolver, AstVoidType* type)
{
	if (type->typeID = GetVoidType())
		return true;
	return false;
}

static bool ResolveIntegerType(Resolver* resolver, AstIntegerType* type)
{
	if (type->typeID = GetIntegerType(type->bitWidth, type->isSigned))
		return true;
	return false;
}

static bool ResolveFPType(Resolver* resolver, AstFPType* type)
{
	switch (type->bitWidth)
	{
	case 16:
		type->typeID = GetFPType(FP_PRECISION_HALF);
		return true;
	case 32:
		type->typeID = GetFPType(FP_PRECISION_SINGLE);
		return true;
	case 64:
		type->typeID = GetFPType(FP_PRECISION_DOUBLE);
		return true;
	case 128:
		type->typeID = GetFPType(FP_PRECISION_FP128);
		return true;
	default:
		SnekAssert(false);
		return false;
	}
}

static bool ResolveBoolType(Resolver* resolver, AstBoolType* type)
{
	if (type->typeID = GetBoolType())
		return true;
	return false;
}

static bool ResolveNamedType(Resolver* resolver, AstNamedType* type)
{
	if (AstStruct* structDecl = FindStruct(resolver, type->name))
	{
		type->typeKind = TYPE_KIND_STRUCT;
		type->typeID = structDecl->type;
		type->structDecl = structDecl;
		return true;
	}
	else if (AstClass* classDecl = FindClass(resolver, type->name))
	{
		type->typeKind = TYPE_KIND_CLASS;
		type->typeID = classDecl->type;
		type->classDecl = classDecl;
		return true;
	}
	else if (AstTypedef* typedefDecl = FindTypedef(resolver, type->name))
	{
		type->typeKind = TYPE_KIND_ALIAS;
		type->typeID = typedefDecl->type;
		type->typedefDecl = typedefDecl;
		return true;
	}
	else if (AstEnum* enumDecl = FindEnum(resolver, type->name))
	{
		type->typeKind = TYPE_KIND_ALIAS;
		type->typeID = enumDecl->type;
		type->enumDecl = enumDecl;
		return true;
	}

	SnekError(resolver->context, type->inputState, ERROR_CODE_UNDEFINED_TYPE, "Undefined type '%s'", type->name);
	return false;
}

static bool ResolvePointerType(Resolver* resolver, AstPointerType* type)
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

static bool ResolveFunctionType(Resolver* resolver, AstFunctionType* type)
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

	type->typeID = GetFunctionType(type->returnType->typeID, numParams, paramTypes, varArgs, false, NULL);

	return result;
}

static bool ResolveArrayType(Resolver* resolver, AstArrayType* type)
{
	if (ResolveType(resolver, type->elementType))
	{
		if (type->length)
		{
			if (ResolveExpression(resolver, type->length))
			{
				if (type->length->type->typeKind == TYPE_KIND_INTEGER && IsConstant(type->length))
				{
					bool success = true;
					type->typeID = GetArrayType(type->elementType->typeID, (int)ConstantFoldInt(resolver, type->length, success));
					return success;
				}
				else
				{
					SnekError(resolver->context, type->length->inputState, ERROR_CODE_ARRAY_LENGTH_WRONG_TYPE, "Array length specifier must be a constant integer value");
					return false;
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

static bool ResolveStringType(Resolver* resolver, AstStringType* type)
{
	type->typeID = GetStringType();
	return true;
}

static bool ResolveType(Resolver* resolver, AstType* type)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = type;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	switch (type->typeKind)
	{
	case TYPE_KIND_VOID:
		return ResolveVoidType(resolver, (AstVoidType*)type);
	case TYPE_KIND_INTEGER:
		return ResolveIntegerType(resolver, (AstIntegerType*)type);
	case TYPE_KIND_FP:
		return ResolveFPType(resolver, (AstFPType*)type);
	case TYPE_KIND_BOOL:
		return ResolveBoolType(resolver, (AstBoolType*)type);
	case TYPE_KIND_NAMED_TYPE:
		return ResolveNamedType(resolver, (AstNamedType*)type);
	case TYPE_KIND_POINTER:
		return ResolvePointerType(resolver, (AstPointerType*)type);
	case TYPE_KIND_FUNCTION:
		return ResolveFunctionType(resolver, (AstFunctionType*)type);

	case TYPE_KIND_ARRAY:
		return ResolveArrayType(resolver, (AstArrayType*)type);
	case TYPE_KIND_STRING:
		return ResolveStringType(resolver, (AstStringType*)type);

	default:
		SnekAssert(false);
		return false;
	}
}

static TypeID GetFittingTypeForIntegerLiteral(Resolver* resolver, AstIntegerLiteral* expr)
{
	bool isSigned = expr->value < 0;
	if (isSigned)
	{
		if (-expr->value <= INT8_MAX)
			return GetIntegerType(8, isSigned);
		else if (-expr->value <= INT16_MAX)
			return GetIntegerType(16, isSigned);
		else if (-expr->value <= INT32_MAX)
			return GetIntegerType(32, isSigned);
		else if (-expr->value <= INT64_MAX)
			return GetIntegerType(64, isSigned);
		else
		{
			SnekAssert(false);
			return NULL;
		}
	}
	else
	{
		if (expr->value <= UINT8_MAX)
			return GetIntegerType(8, isSigned);
		else if (expr->value <= UINT16_MAX)
			return GetIntegerType(16, isSigned);
		else if (expr->value <= UINT32_MAX)
			return GetIntegerType(32, isSigned);
		else if (expr->value <= UINT64_MAX)
			return GetIntegerType(64, isSigned);
		else
		{
			SnekAssert(false);
			return NULL;
		}
	}
}

static bool ResolveIntegerLiteral(Resolver* resolver, AstIntegerLiteral* expr)
{
	expr->type = GetFittingTypeForIntegerLiteral(resolver, expr);
	expr->lvalue = false;
	return true;
}

static bool ResolveFPLiteral(Resolver* resolver, AstFPLiteral* expr)
{
	expr->type = GetFPType(FP_PRECISION_SINGLE);
	expr->lvalue = false;
	return true;
}

static bool ResolveBoolLiteral(Resolver* resolver, AstBoolLiteral* expr)
{
	expr->type = GetBoolType();
	expr->lvalue = false;
	return true;
}

static bool ResolveCharacterLiteral(Resolver* resolver, AstCharacterLiteral* expr)
{
	expr->type = GetIntegerType(8, false);
	expr->lvalue = false;
	return true;
}

static bool ResolveNullLiteral(Resolver* resolver, AstNullLiteral* expr)
{
	expr->type = GetPointerType(GetVoidType());
	expr->lvalue = false;
	return true;
}

static bool ResolveStringLiteral(Resolver* resolver, AstStringLiteral* expr)
{
	expr->type = GetStringType();
	//expr->type = GetPointerType(GetIntegerType(8, true));
	expr->lvalue = false;
	return true;
}

static bool ResolveStructLiteral(Resolver* resolver, AstStructLiteral* expr)
{
	bool result = true;

	if (ResolveType(resolver, expr->structType))
	{
		TypeID* valueTypes = new TypeID[expr->values.size];
		for (int i = 0; i < expr->values.size; i++)
		{
			if (ResolveExpression(resolver, expr->values[i]))
			{
				//if (IsConstant(expr->values[i]))
				//{
				valueTypes[i] = expr->values[i]->type;
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

		expr->type = expr->structType->typeID;
		expr->lvalue = false;
	}
	else
	{
		result = false;
	}

	return result;
}

static bool ResolveIdentifier(Resolver* resolver, AstIdentifier* expr)
{
	if (AstVariable* variable = FindVariable(resolver, expr->name))
	{
		expr->type = variable->type;
		expr->lvalue = true; // !variable->isConstant;
		expr->variable = variable;
		return true;
	}
	if (AstFunction* function = FindFunction(resolver, expr->name))
	{
		expr->type = function->type;
		expr->lvalue = false;
		expr->function = function;
		return true;
	}
	if (AstEnumValue* enumValue = FindEnumValue(resolver, expr->name))
	{
		expr->type = enumValue->enumDecl->type;
		expr->lvalue = false;
		expr->enumValue = enumValue;
		return true;
	}
	if (AstExprdef* exprdef = FindExprdef(resolver, expr->name))
	{
		AstExpression* value = CopyExpression(exprdef->alias, resolver->file);

		if (ResolveExpression(resolver, value))
		{
			expr->type = value->type;
			expr->lvalue = value->lvalue;
			expr->exprdefValue = value;
			return true;
		}
	}

	SnekError(resolver->context, expr->inputState, ERROR_CODE_UNRESOLVED_IDENTIFIER, "Unresolved identifier '%s'", expr->name);
	return false;
}

static bool ResolveCompoundExpression(Resolver* resolver, AstCompoundExpression* expr)
{
	if (ResolveExpression(resolver, expr->value))
	{
		expr->type = expr->value->type;
		expr->lvalue = expr->value->lvalue;
		return true;
	}
	return false;
}

static bool CanConvert(TypeID argType, TypeID paramType)
{
	if (CompareTypes(argType, paramType))
		return true;

	while (argType->typeKind == TYPE_KIND_ALIAS)
		argType = argType->aliasType.alias;
	while (paramType->typeKind == TYPE_KIND_ALIAS)
		paramType = paramType->aliasType.alias;

	if (argType->typeKind == TYPE_KIND_INTEGER)
	{
		if (paramType->typeKind == TYPE_KIND_INTEGER)
			return true;
		else if (paramType->typeKind == TYPE_KIND_BOOL)
			return true;
		else if (paramType->typeKind == TYPE_KIND_FP)
			return true;
		else if (paramType->typeKind == TYPE_KIND_POINTER)
			return true;
	}
	else if (argType->typeKind == TYPE_KIND_FP)
	{
		if (paramType->typeKind == TYPE_KIND_FP)
			return true;
		else if (paramType->typeKind == TYPE_KIND_INTEGER)
			return true;
	}
	else if (argType->typeKind == TYPE_KIND_BOOL)
	{
		if (paramType->typeKind == TYPE_KIND_INTEGER)
			return true;
	}
	else if (argType->typeKind == TYPE_KIND_POINTER)
	{
		if (paramType->typeKind == TYPE_KIND_POINTER)
			return true;
		else if (paramType->typeKind == TYPE_KIND_CLASS)
			return true;
		else if (paramType->typeKind == TYPE_KIND_FUNCTION)
			return true;
		else if (paramType->typeKind == TYPE_KIND_INTEGER)
			return true;
		else if (paramType->typeKind == TYPE_KIND_STRING)
			return true;
	}
	else if (argType->typeKind == TYPE_KIND_FUNCTION)
	{
		if (paramType->typeKind == TYPE_KIND_FUNCTION)
			return true;
		else if (paramType->typeKind == TYPE_KIND_POINTER)
			return true;
	}
	else if (argType->typeKind == TYPE_KIND_STRING)
	{
		if (paramType->typeKind == TYPE_KIND_POINTER)
			return true;
	}

	return false;
}

static bool CanConvertImplicit(TypeID argType, TypeID paramType, bool argIsConstant)
{
	argType = UnwrapType(argType);
	paramType = UnwrapType(paramType);

	if (CompareTypes(argType, paramType))
		return true;

	while (argType->typeKind == TYPE_KIND_ALIAS)
		argType = argType->aliasType.alias;
	while (paramType->typeKind == TYPE_KIND_ALIAS)
		paramType = paramType->aliasType.alias;

	if (argType->typeKind == TYPE_KIND_INTEGER && paramType->typeKind == TYPE_KIND_INTEGER)
	{
		if (argType->integerType.bitWidth == paramType->integerType.bitWidth)
			return true;
		else if (argType->integerType.bitWidth <= paramType->integerType.bitWidth)
			//return argIsConstant;
			return true;
	}
	else if (argType->typeKind == TYPE_KIND_INTEGER && paramType->typeKind == TYPE_KIND_BOOL)
	{
		return true;
	}
	else if (argType->typeKind == TYPE_KIND_BOOL && paramType->typeKind == TYPE_KIND_INTEGER)
	{
		if (argIsConstant)
			return true;
	}
	else if (argType->typeKind == TYPE_KIND_FP && paramType->typeKind == TYPE_KIND_FP)
	{
		if (argIsConstant)
			return true;
	}
	else if (argType->typeKind == TYPE_KIND_POINTER && paramType->typeKind == TYPE_KIND_POINTER)
	{
		if (argIsConstant || argType->pointerType.elementType->typeKind == TYPE_KIND_VOID || paramType->pointerType.elementType->typeKind == TYPE_KIND_VOID)
			return true;
	}
	else if (argType->typeKind == TYPE_KIND_POINTER && paramType->typeKind == TYPE_KIND_CLASS)
	{
		if (argIsConstant)
			return true;
	}
	else if (argType->typeKind == TYPE_KIND_POINTER && paramType->typeKind == TYPE_KIND_FUNCTION)
	{
		if (argIsConstant)
			return true;
	}
	else if (argType->typeKind == TYPE_KIND_POINTER && paramType->typeKind == TYPE_KIND_STRING)
	{
		//if (argIsConstant)
		return true;
	}
	else if (argType->typeKind == TYPE_KIND_STRING && paramType->typeKind == TYPE_KIND_POINTER && paramType->pointerType.elementType->typeKind == TYPE_KIND_INTEGER && paramType->pointerType.elementType->integerType.bitWidth == 8)
	{
		//if (argIsConstant)
		return true;
	}

	return false;
}

static bool ResolveFunctionCall(Resolver* resolver, AstFuncCall* expr)
{
	bool result = true;

	TypeID functionType = NULL;
	AstFunction* function = NULL;

	if (ResolveExpression(resolver, expr->calleeExpr))
	{
		functionType = expr->calleeExpr->type;
		while (functionType->typeKind == TYPE_KIND_ALIAS)
			functionType = functionType->aliasType.alias;
		if (functionType->typeKind == TYPE_KIND_FUNCTION)
		{
			function = functionType->functionType.declaration;

			expr->function = function;
			expr->type = functionType->functionType.returnType;
			expr->lvalue = false;

			if (expr->calleeExpr->exprKind == EXPR_KIND_DOT_OPERATOR)
			{
				AstDotOperator* dotOperator = (AstDotOperator*)expr->calleeExpr;
				if (dotOperator->classMethod)
				{
					expr->isMethodCall = true;
					expr->methodInstance = ((AstDotOperator*)expr->calleeExpr)->operand;

					if (!expr->methodInstance->lvalue && expr->methodInstance->type->typeKind != TYPE_KIND_POINTER)
					{
						SnekError(resolver->context, expr->inputState, ERROR_CODE_RETURN_WRONG_TYPE, "Instance for method call must be an lvalue or a reference");
						result = false;
					}

					if (expr->arguments.size < functionType->functionType.numParams - 1 || expr->arguments.size > functionType->functionType.numParams - 1 && !functionType->functionType.varArgs)
					{
						if (function)
							SnekError(resolver->context, expr->inputState, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments when calling function %s: should be %d instead of %d", function->name, function->paramTypes.size - 1, expr->arguments.size);
						else
							SnekError(resolver->context, expr->inputState, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments in function call: should be %d instead of %d", functionType->functionType.numParams - 1, expr->arguments.size);
						result = false;
					}
				}
				else if (dotOperator->namespacedFunction)
				{
					//expr->function = dotOperator->namespacedFunction;
					//expr->type = dotOperator->namespacedFunction->returnType->typeID;
					//expr->lvalue = false;
					//expr->isMethodCall = false;

					//functionType = dotOperator->namespacedFunction->type;
					//functionAst = dotOperator->namespacedFunction;
				}
			}
			else
			{
				if (expr->arguments.size < functionType->functionType.numParams || expr->arguments.size > functionType->functionType.numParams && !functionType->functionType.varArgs)
				{
					if (function)
						SnekError(resolver->context, expr->inputState, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments when calling function %s: should be %d instead of %d", function->name, function->paramTypes.size, expr->arguments.size);
					else
						SnekError(resolver->context, expr->inputState, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments in function call: should be %d instead of %d", functionType->functionType.numParams, expr->arguments.size);
					result = false;
				}
			}
		}
		else
		{
			SnekError(resolver->context, expr->calleeExpr->inputState, ERROR_CODE_NON_INVOKABLE_EXPRESSION, "Can't invoke expression %.16s", expr->calleeExpr->inputState.ptr);
			result = false;
		}
	}
	else
	{
		result = false;
	}

	/*
	if (expr->calleeExpr->exprKind == EXPR_KIND_IDENTIFIER)
	{
		if (AstFunction* function = FindFunction(resolver, ((AstIdentifier*)expr->calleeExpr)->name))
		{
			if (expr->arguments.size < function->paramTypes.size || expr->arguments.size > function->paramTypes.size && !function->varArgs)
			{
				SnekError(resolver->context, expr->inputState, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments in function call: should be %d instead of %d", function->paramTypes.size, expr->arguments.size);
				result = false;
			}

			expr->function = function;
			expr->type = function->returnType->typeID;
			expr->lvalue = false;

			functionType = function->type;
			functionAst = function;
		}
	}
	else if (expr->calleeExpr->exprKind == EXPR_KIND_DOT_OPERATOR)
	{
		if (ResolveExpression(resolver, expr->calleeExpr))
		{
			AstDotOperator* dotOperator = (AstDotOperator*)expr->calleeExpr;
			if (dotOperator->classMethod)
			{
				AstFunction* method = dotOperator->classMethod;
				if (expr->arguments.size < method->paramTypes.size || expr->arguments.size > method->paramTypes.size && !method->varArgs)
				{
					SnekError(resolver->context, expr->inputState, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments in method call: should be %d instead of %d", method->paramTypes.size, expr->arguments.size);
					result = false;
				}

				expr->function = dotOperator->classMethod;
				expr->type = dotOperator->classMethod->returnType->typeID;
				expr->lvalue = false;
				expr->isMethodCall = true;
				expr->methodInstance = dotOperator->operand;

				functionType = dotOperator->classMethod->type;
				functionAst = dotOperator->classMethod;
			}
			else if (dotOperator->nameSpace)
			{
				expr->function = dotOperator->namespacedFunction;
				expr->type = dotOperator->namespacedFunction->returnType->typeID;
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
			if (expr->calleeExpr->type->typeKind == TYPE_KIND_FUNCTION)
			{
				expr->type = expr->calleeExpr->type->functionType.returnType;
				expr->lvalue = false;

				functionType = expr->type;
				functionAst = NULL;
			}
			else
			{
				SnekError(resolver->context, expr->calleeExpr->inputState, ERROR_CODE_NON_INVOKABLE_EXPRESSION, "Can't invoke expression %.16s", expr->calleeExpr->inputState.ptr);
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

		for (int i = 0; i < expr->arguments.size; i++)
		{
			if (ResolveExpression(resolver, expr->arguments[i]))
			{
				int paramOffset = expr->isMethodCall ? 1 : 0;
				int paramCount = functionType->functionType.numParams - (expr->isMethodCall ? 1 : 0);

				if (i < paramCount)
				{
					TypeID argType = expr->arguments[i]->type;
					TypeID paramType = functionType->functionType.paramTypes[paramOffset + i];
					bool argIsConstant = IsConstant(expr->arguments[i]);

					if (!CanConvertImplicit(argType, paramType, argIsConstant))
					{
						result = false;

						const char* argTypeStr = GetTypeString(argType);
						const char* paramTypeStr = GetTypeString(paramType);

						if (function)
						{
							SnekError(resolver->context, expr->inputState, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong type of argument #%d '%s': should be %s instead of %s", i + 1, function->paramNames[paramOffset + i], paramTypeStr, argTypeStr);
						}
						else
						{
							SnekError(resolver->context, expr->inputState, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong type of argument #%d: should be %s instead of %s", i + 1, paramTypeStr, argTypeStr);
						}
					}
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

static bool ResolveSubscriptOperator(Resolver* resolver, AstSubscriptOperator* expr)
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
			if (expr->operand->type->typeKind == TYPE_KIND_POINTER)
			{
				if (expr->arguments.size == 1)
				{
					if (expr->operand->type->pointerType.elementType->typeKind == TYPE_KIND_ARRAY)
					{
						expr->type = expr->operand->type->pointerType.elementType->arrayType.elementType;
					}
					else
					{
						expr->type = expr->operand->type->pointerType.elementType;
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
			else if (expr->operand->type->typeKind == TYPE_KIND_ARRAY)
			{
				if (expr->arguments.size == 1)
				{
					expr->type = expr->operand->type->arrayType.elementType;
					expr->lvalue = true;
					return true;
				}
				else
				{
					SnekAssert(false);
					return false;
				}
			}
			else if (expr->operand->type->typeKind == TYPE_KIND_STRING)
			{
				if (expr->arguments.size == 1)
				{
					expr->type = GetIntegerType(8, true);
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

static bool ResolveDotOperator(Resolver* resolver, AstDotOperator* expr)
{
	if (expr->operand->exprKind == EXPR_KIND_IDENTIFIER)
	{
		const char* nameSpace = ((AstIdentifier*)expr->operand)->name;

		if (AstFile* file = FindFileWithNamespace(resolver, nameSpace, resolver->file))
		{
			if (AstFunction* function = FindFunctionInFile(resolver, file, expr->name))
			{
				if (function->visibility >= VISIBILITY_PUBLIC || file->module == resolver->file->module)
				{
					expr->namespacedFunction = function;
					expr->type = function->type;
					expr->lvalue = false;
					return true;
				}
				else
				{
					SnekError(resolver->context, expr->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Function '%s' is not visible", function->name);
					return false;
				}
			}
			if (AstVariable* variable = FindGlobalVariableInFile(resolver, file, expr->name))
			{
				expr->namespacedVariable = variable;
				expr->type = variable->type;
				expr->lvalue = true;
				return true;
			}
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
					expr->type = function->type;
					expr->lvalue = false;
					return true;
				}
				if (AstVariable* variable = FindGlobalVariableInNamespace(resolver, ns, expr->name, resolver->file->module))
				{
					expr->namespacedVariable = variable;
					expr->type = variable->type;
					expr->lvalue = true;
					return true;
				}
				if (AstModule* nestedNs = FindModule(resolver, expr->name, ns))
				{
					expr->ns = nestedNs;
					expr->type = NULL;
					return true;
				}

				SnekError(resolver->context, expr->inputState, ERROR_CODE_UNRESOLVED_IDENTIFIER, "Unresolved identifier '%s' in module %s", expr->name, ns->name);
				return false;
			}
		}
		*/
	}
	if (ResolveExpression(resolver, expr->operand))
	{
		if (expr->operand->exprKind == EXPR_KIND_DOT_OPERATOR && ((AstDotOperator*)expr->operand)->ns)
		{
			AstModule* ns = ((AstDotOperator*)expr->operand)->ns;
			for (int i = 0; i < ns->files.size; i++)
			{
				if (AstFunction* function = FindFunctionInFile(resolver, ns->files[i], expr->name))
				{
					expr->namespacedFunction = function;
					expr->type = function->type;
					expr->lvalue = false;
					return true;
				}
			}
			if (AstModule* nestedNs = FindModule(resolver, expr->name, ns))
			{
				expr->ns = nestedNs;
				expr->type = NULL;
				return true;
			}

			SnekError(resolver->context, expr->inputState, ERROR_CODE_UNRESOLVED_IDENTIFIER, "Unresolved identifier '%s' in namespace %s", expr->name, ns->name);
			return false;
		}

		TypeID operandType = expr->operand->type;

		while (operandType->typeKind == TYPE_KIND_ALIAS)
			operandType = operandType->aliasType.alias;

		if (operandType->typeKind == TYPE_KIND_POINTER)
			operandType = operandType->pointerType.elementType;

		while (operandType->typeKind == TYPE_KIND_ALIAS)
			operandType = operandType->aliasType.alias;

		if (operandType->typeKind == TYPE_KIND_STRUCT)
		{
			for (int i = 0; i < operandType->structType.declaration->fields.size; i++)
			{
				AstStructField* field = &operandType->structType.declaration->fields[i];
				TypeID memberType = operandType->structType.fieldTypes[i];
				if (strcmp(field->name, expr->name) == 0)
				{
					expr->type = memberType;
					expr->lvalue = true;
					expr->structField = field;
					return true;
				}
			}

			if (AstFunction* method = FindFunction(resolver, expr->name))
			{
				expr->type = method->type;
				expr->lvalue = false;
				expr->classMethod = method;
				return true;
			}

			SnekError(resolver->context, expr->inputState, ERROR_CODE_UNRESOLVED_MEMBER, "Unresolved struct member %s.%s", GetTypeString(expr->operand->type), expr->name);
			return false;
		}
		else if (operandType->typeKind == TYPE_KIND_CLASS)
		{
			for (int i = 0; i < operandType->classType.declaration->fields.size; i++)
			{
				AstClassField* field = &operandType->classType.declaration->fields[i];
				TypeID memberType = operandType->classType.fieldTypes[i];
				if (strcmp(field->name, expr->name) == 0)
				{
					expr->type = memberType;
					expr->lvalue = true;
					expr->classField = field;
					return true;
				}
			}
			for (int i = 0; i < operandType->classType.declaration->methods.size; i++)
			{
				AstFunction* method = operandType->classType.declaration->methods[i];
				if (strcmp(method->name, expr->name) == 0)
				{
					expr->type = method->type;
					expr->lvalue = false;
					expr->classMethod = method;
					return true;
				}
			}

			SnekError(resolver->context, expr->inputState, ERROR_CODE_UNRESOLVED_MEMBER, "Unresolved class member %s.%s", GetTypeString(expr->operand->type), expr->name);
			return false;
		}
		else if (operandType->typeKind == TYPE_KIND_ARRAY)
		{
			expr->arrayField = -1;
			if (strcmp(expr->name, "length") == 0)
			{
				expr->type = GetIntegerType(32, false);
				expr->lvalue = false;
				expr->arrayField = 0;
				return true;
			}
			else if (strcmp(expr->name, "buffer") == 0)
			{
				expr->type = GetPointerType(expr->operand->type->arrayType.elementType);
				expr->lvalue = false;
				expr->arrayField = 1;
				return true;
			}

			SnekError(resolver->context, expr->inputState, ERROR_CODE_UNRESOLVED_MEMBER, "Unresolved array property %s.%s", GetTypeString(expr->operand->type), expr->name);
			return false;
		}
		else if (operandType->typeKind == TYPE_KIND_STRING)
		{
			expr->stringField = -1;
			if (strcmp(expr->name, "length") == 0)
			{
				expr->type = GetIntegerType(32, false);
				expr->lvalue = false;
				expr->stringField = 0;
				return true;
			}
			else if (strcmp(expr->name, "buffer") == 0)
			{
				expr->type = GetPointerType(GetIntegerType(8, true));
				expr->lvalue = false;
				expr->stringField = 1;
				return true;
			}

			SnekError(resolver->context, expr->inputState, ERROR_CODE_UNRESOLVED_MEMBER, "Unresolved string property %s.%s", GetTypeString(expr->operand->type), expr->name);
			return false;
		}
		else
		{
			SnekError(resolver->context, expr->inputState, ERROR_CODE_UNRESOLVED_MEMBER, "Unresolved property %s.%s", GetTypeString(expr->operand->type), expr->name);
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

static bool ResolveCast(Resolver* resolver, AstCast* expr)
{
	if (ResolveType(resolver, expr->castType))
	{
		if (ResolveExpression(resolver, expr->value))
		{
			if (CanConvert(expr->value->type, expr->castType->typeID))
			{
				expr->type = expr->castType->typeID;
				expr->lvalue = false;
				return true;
			}
			else
			{
				SnekError(resolver->context, expr->inputState, ERROR_CODE_INVALID_CAST, "Invalid cast: %s to %s", GetTypeString(expr->value->type), GetTypeString(expr->castType->typeID));
			}
		}
	}
	return false;
}

static bool ResolveSizeof(Resolver* resolver, AstSizeof* expr)
{
	if (ResolveType(resolver, expr->sizedType))
	{
		expr->type = GetIntegerType(64, false);
		expr->lvalue = false;
		return true;
	}
	return false;
}

static bool ResolveMalloc(Resolver* resolver, AstMalloc* expr)
{
	bool result = true;

	if (ResolveType(resolver, expr->mallocType))
	{
		if (expr->mallocType->typeKind == TYPE_KIND_CLASS)
		{
			if (AstFunction* constructor = expr->mallocType->typeID->classType.declaration->constructor)
			{
				if (expr->arguments.size < constructor->paramTypes.size || expr->arguments.size > constructor->paramTypes.size && !constructor->varArgs)
				{
					SnekError(resolver->context, expr->inputState, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong number of arguments in constructor: should be %d instead of %d", constructor->paramTypes.size, expr->arguments.size);
					result = false;
				}
				for (int i = 0; i < expr->arguments.size; i++)
				{
					if (ResolveExpression(resolver, expr->arguments[i]))
					{
						if (i < constructor->paramTypes.size)
						{
							TypeID argType = expr->arguments[i]->type;
							TypeID paramType = constructor->paramTypes[i]->typeID;
							const char* argTypeStr = GetTypeString(expr->arguments[i]->type);
							const char* paramTypeStr = GetTypeString(constructor->paramTypes[i]->typeID);
							if (!CanConvertImplicit(argType, paramType, IsConstant(expr->arguments[i])))
							{
								SnekError(resolver->context, expr->arguments[i]->inputState, ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH, "Wrong type of argument '%s': should be %s instead of %s", constructor->paramNames[i], paramTypeStr, argTypeStr);
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

			expr->type = expr->mallocType->typeID;
			expr->lvalue = false;
		}
		else
		{
			if (expr->hasArguments)
			{
				SnekError(resolver->context, expr->mallocType->inputState, ERROR_CODE_MALLOC_INVALID_TYPE, "Can't call constructor of non-class type");
				result = false;
			}
			else
			{
				expr->type = GetPointerType(expr->mallocType->typeID);
				expr->lvalue = false;
			}
		}
		if (expr->count)
		{
			if (!ResolveExpression(resolver, expr->count))
				result = false;
		}
	}
	else
	{
		result = false;
	}
	return result;
}

static bool ResolveUnaryOperator(Resolver* resolver, AstUnaryOperator* expr)
{
	if (ResolveExpression(resolver, expr->operand))
	{
		if (!expr->position)
		{
			switch (expr->operatorType)
			{
			case UNARY_OPERATOR_NOT:
				expr->type = GetBoolType();
				expr->lvalue = false;
				return true;
			case UNARY_OPERATOR_NEGATE:
				expr->type = expr->operand->type;
				expr->lvalue = false;
				return true;
			case UNARY_OPERATOR_REFERENCE:
				expr->type = GetPointerType(expr->operand->type);
				expr->lvalue = false;
				return true;
			case UNARY_OPERATOR_DEREFERENCE:
				if (expr->operand->type->typeKind == TYPE_KIND_POINTER)
				{
					expr->type = expr->operand->type->pointerType.elementType;
					expr->lvalue = true;
					return true;
				}
				else
				{
					SnekError(resolver->context, expr->operand->inputState, ERROR_CODE_DEREFERENCE_INVALID_TYPE, "Can't dereference non-pointer type value");
					return false;
				}

			case UNARY_OPERATOR_INCREMENT:
				expr->type = expr->operand->type;
				expr->lvalue = true;
				return true;
			case UNARY_OPERATOR_DECREMENT:
				expr->type = expr->operand->type;
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
			case UNARY_OPERATOR_INCREMENT:
				expr->type = expr->operand->type;
				expr->lvalue = false;
				return true;
			case UNARY_OPERATOR_DECREMENT:
				expr->type = expr->operand->type;
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
	while (leftType->typeKind == TYPE_KIND_ALIAS)
		leftType = leftType->aliasType.alias;
	while (rightType->typeKind == TYPE_KIND_ALIAS)
		rightType = rightType->aliasType.alias;

	if (leftType->typeKind == TYPE_KIND_INTEGER && rightType->typeKind == TYPE_KIND_INTEGER)
	{
		int bitWidth = max(leftType->integerType.bitWidth, rightType->integerType.bitWidth);
		bool isSigned = leftType->integerType.isSigned || rightType->integerType.isSigned;
		return GetIntegerType(bitWidth, isSigned);
	}
	else if (leftType->typeKind == TYPE_KIND_FP && rightType->typeKind == TYPE_KIND_FP)
	{
		FPTypePrecision precision = (FPTypePrecision)max((int)leftType->fpType.precision, (int)rightType->fpType.precision);
		return GetFPType(precision);
	}
	else if (leftType->typeKind == TYPE_KIND_INTEGER && rightType->typeKind == TYPE_KIND_FP)
	{
		return rightType;
	}
	else if (leftType->typeKind == TYPE_KIND_FP && rightType->typeKind == TYPE_KIND_INTEGER)
	{
		return leftType;
	}
	else if (leftType->typeKind == TYPE_KIND_POINTER && rightType->typeKind == TYPE_KIND_INTEGER)
	{
		return leftType;
	}
	else if (leftType->typeKind == TYPE_KIND_POINTER && rightType->typeKind == TYPE_KIND_POINTER)
	{
		if (leftType->pointerType.elementType->typeKind == TYPE_KIND_VOID)
			return rightType;
		if (rightType->pointerType.elementType->typeKind == TYPE_KIND_VOID)
			return leftType;
		SnekAssert(false);
		return nullptr;
	}
	else
	{
		SnekAssert(false);
		return nullptr;
	}
}

static bool ResolveBinaryOperator(Resolver* resolver, AstBinaryOperator* expr)
{
	bool result = true;

	result = ResolveExpression(resolver, expr->left) && result;
	result = ResolveExpression(resolver, expr->right) && result;

	if (result)
	{
		switch (expr->operatorType)
		{
		case BINARY_OPERATOR_ADD:
		case BINARY_OPERATOR_SUB:
		case BINARY_OPERATOR_MUL:
		case BINARY_OPERATOR_DIV:
		case BINARY_OPERATOR_MOD:
			expr->type = BinaryOperatorTypeMeet(resolver, expr->left->type, expr->right->type);
			expr->lvalue = false;
			return true;

		case BINARY_OPERATOR_EQ:
		case BINARY_OPERATOR_NE:
		case BINARY_OPERATOR_LT:
		case BINARY_OPERATOR_GT:
		case BINARY_OPERATOR_LE:
		case BINARY_OPERATOR_GE:
			expr->type = GetBoolType();
			expr->lvalue = false;
			return true;

		case BINARY_OPERATOR_AND:
		case BINARY_OPERATOR_OR:
			if (CanConvertImplicit(expr->left->type, GetBoolType(), false) && CanConvertImplicit(expr->right->type, GetBoolType(), false))
			{
				expr->type = GetBoolType();
				expr->lvalue = false;
				return true;
			}
			else
			{
				if (expr->operatorType == BINARY_OPERATOR_AND)
					SnekError(resolver->context, expr->left->inputState, ERROR_CODE_BIN_OP_INVALID_TYPE, "Operands of && must be of bool type");
				else if (expr->operatorType == BINARY_OPERATOR_OR)
					SnekError(resolver->context, expr->left->inputState, ERROR_CODE_BIN_OP_INVALID_TYPE, "Operands of || must be of bool type");
				else
					SnekAssert(false);
				return false;
			}

		case BINARY_OPERATOR_BITWISE_AND:
		case BINARY_OPERATOR_BITWISE_OR:
		case BINARY_OPERATOR_BITWISE_XOR:
		case BINARY_OPERATOR_BITSHIFT_LEFT:
		case BINARY_OPERATOR_BITSHIFT_RIGHT:
			expr->type = BinaryOperatorTypeMeet(resolver, expr->left->type, expr->right->type);
			expr->lvalue = false;
			return true;

		case BINARY_OPERATOR_ASSIGN:
		case BINARY_OPERATOR_ADD_ASSIGN:
		case BINARY_OPERATOR_SUB_ASSIGN:
		case BINARY_OPERATOR_MUL_ASSIGN:
		case BINARY_OPERATOR_DIV_ASSIGN:
		case BINARY_OPERATOR_MOD_ASSIGN:
		case BINARY_OPERATOR_BITSHIFT_LEFT_ASSIGN:
		case BINARY_OPERATOR_BITSHIFT_RIGHT_ASSIGN:
		case BINARY_OPERATOR_BITWISE_AND_ASSIGN:
		case BINARY_OPERATOR_BITWISE_OR_ASSIGN:
		case BINARY_OPERATOR_BITWISE_XOR_ASSIGN:
			if (expr->left->lvalue)
			{
				if (CanConvertImplicit(expr->right->type, expr->left->type, IsConstant(expr->right)))
				{
					expr->type = expr->left->type;
					expr->lvalue = true;
					return true;
				}
				else
				{
					SnekError(resolver->context, expr->left->inputState, ERROR_CODE_BIN_OP_INVALID_TYPE, "Can't assign value of type '%s' to variable of type '%s'", GetTypeString(expr->right->type), GetTypeString(expr->left->type));
					return false;
				}
			}
			else
			{
				SnekError(resolver->context, expr->left->inputState, ERROR_CODE_BIN_OP_INVALID_TYPE, "Can't assign to a non lvalue");
				return false;
			}

		default:
			SnekAssert(false);
			return NULL;
		}
	}
	return false;
}

static bool ResolveTernaryOperator(Resolver* resolver, AstTernaryOperator* expr)
{
	if (ResolveExpression(resolver, expr->condition) && ResolveExpression(resolver, expr->thenValue) && ResolveExpression(resolver, expr->elseValue))
	{
		TypeID type = BinaryOperatorTypeMeet(resolver, expr->thenValue->type, expr->elseValue->type);
		//if (CompareTypes(expr->thenValue->type, expr->elseValue->type))
		if (type)
		{
			expr->type = type;
			expr->lvalue = false;
			return true;
		}
		else
		{
			SnekError(resolver->context, expr->inputState, ERROR_CODE_TERNARY_OPERATOR_TYPE_MISMATCH, "Values of ternary operator must be of the same type: %s and %s", GetTypeString(expr->thenValue->type), GetTypeString(expr->elseValue->type));
			return false;
		}
	}
	return false;
}

static bool ResolveExpression(Resolver* resolver, AstExpression* expression)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = expression;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	switch (expression->exprKind)
	{
	case EXPR_KIND_INTEGER_LITERAL:
		return ResolveIntegerLiteral(resolver, (AstIntegerLiteral*)expression);
	case EXPR_KIND_FP_LITERAL:
		return ResolveFPLiteral(resolver, (AstFPLiteral*)expression);
	case EXPR_KIND_BOOL_LITERAL:
		return ResolveBoolLiteral(resolver, (AstBoolLiteral*)expression);
	case EXPR_KIND_CHARACTER_LITERAL:
		return ResolveCharacterLiteral(resolver, (AstCharacterLiteral*)expression);
	case EXPR_KIND_NULL_LITERAL:
		return ResolveNullLiteral(resolver, (AstNullLiteral*)expression);
	case EXPR_KIND_STRING_LITERAL:
		return ResolveStringLiteral(resolver, (AstStringLiteral*)expression);
	case EXPR_KIND_STRUCT_LITERAL:
		return ResolveStructLiteral(resolver, (AstStructLiteral*)expression);
	case EXPR_KIND_IDENTIFIER:
		return ResolveIdentifier(resolver, (AstIdentifier*)expression);
	case EXPR_KIND_COMPOUND:
		return ResolveCompoundExpression(resolver, (AstCompoundExpression*)expression);

	case EXPR_KIND_FUNC_CALL:
		return ResolveFunctionCall(resolver, (AstFuncCall*)expression);
	case EXPR_KIND_SUBSCRIPT_OPERATOR:
		return ResolveSubscriptOperator(resolver, (AstSubscriptOperator*)expression);
	case EXPR_KIND_DOT_OPERATOR:
		return ResolveDotOperator(resolver, (AstDotOperator*)expression);
	case EXPR_KIND_CAST:
		return ResolveCast(resolver, (AstCast*)expression);
	case EXPR_KIND_SIZEOF:
		return ResolveSizeof(resolver, (AstSizeof*)expression);
	case EXPR_KIND_MALLOC:
		return ResolveMalloc(resolver, (AstMalloc*)expression);

	case EXPR_KIND_UNARY_OPERATOR:
		return ResolveUnaryOperator(resolver, (AstUnaryOperator*)expression);
	case EXPR_KIND_BINARY_OPERATOR:
		return ResolveBinaryOperator(resolver, (AstBinaryOperator*)expression);
	case EXPR_KIND_TERNARY_OPERATOR:
		return ResolveTernaryOperator(resolver, (AstTernaryOperator*)expression);

	default:
		SnekAssert(false);
		return NULL;
	}
}

static bool ResolveStatement(Resolver* resolver, AstStatement* statement);

static bool ResolveCompoundStatement(Resolver* resolver, AstCompoundStatement* statement)
{
	bool result = true;

	ResolverPushScope(resolver, "");
	for (int i = 0; i < statement->statements.size; i++)
	{
		result = ResolveStatement(resolver, statement->statements[i]) && result;
	}
	ResolverPopScope(resolver);

	return result;
}

static bool ResolveExprStatement(Resolver* resolver, AstExprStatement* statement)
{
	return ResolveExpression(resolver, statement->expr);
}

static bool ResolveVarDeclStatement(Resolver* resolver, AstVarDeclStatement* statement)
{
	bool result = true;

	if (ResolveType(resolver, statement->type))
	{
		for (int i = 0; i < statement->declarators.size; i++)
		{
			AstVarDeclarator& declarator = statement->declarators[i];

			// Check if variable with that name already exists in the current function
			if (AstVariable* variableWithSameName = FindVariable(resolver, declarator.name))
			{
				SnekWarn(resolver->context, statement->inputState, ERROR_CODE_VARIABLE_SHADOWING, "Variable with name '%s' already exists at %s:%d:%d, will be shadowed", declarator.name, variableWithSameName->inputState.filename, variableWithSameName->inputState.line, variableWithSameName->inputState.col);
			}

			if (declarator.value)
			{
				if (ResolveExpression(resolver, declarator.value))
				{
					if (!CanConvertImplicit(declarator.value->type, statement->type->typeID, IsConstant(declarator.value)))
					{
						SnekError(resolver->context, statement->inputState, ERROR_CODE_BIN_OP_INVALID_TYPE, "Can't assign value of type '%s' to variable of type '%s'", GetTypeString(declarator.value->type), GetTypeString(statement->type->typeID));
						result = false;
						//SnekError(resolver->context, statement->inputState, ERROR_CODE_INVALID_CAST, "Can't assign ");
					}
				}
				else
				{
					result = false;
				}
			}
			declarator.variable = RegisterLocalVariable(resolver, statement->type->typeID, declarator.value, declarator.name, statement->isConstant, resolver->file, declarator.inputState);
		}
	}
	else
	{
		result = false;
	}

	return result;
}

static bool ResolveIfStatement(Resolver* resolver, AstIfStatement* statement)
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

static bool ResolveWhileLoop(Resolver* resolver, AstWhileLoop* statement)
{
	bool result = true;

	ResolverPushScope(resolver, "");
	resolver->scope->branchDst = statement;

	result = ResolveExpression(resolver, statement->condition) && result;
	result = ResolveStatement(resolver, statement->body) && result;

	ResolverPopScope(resolver);

	return result;
}

static bool ResolveForLoop(Resolver* resolver, AstForLoop* statement)
{
	bool result = true;

	ResolverPushScope(resolver, "");
	resolver->scope->branchDst = statement;

	result = ResolveExpression(resolver, statement->startValue) && ResolveExpression(resolver, statement->endValue) && result;
	if (statement->deltaValue)
	{
		result = ResolveExpression(resolver, statement->deltaValue) && result;

		if (IsConstant(statement->deltaValue))
		{
			statement->direction = (int)((AstIntegerLiteral*)statement->deltaValue)->value;
		}
		else
		{
			SnekError(resolver->context, statement->deltaValue->inputState, ERROR_CODE_FOR_LOOP_SYNTAX, "For loop step value must be a constant integer");
			result = false;
		}
	}
	else
	{
		statement->direction = 1;
	}

	statement->iterator = RegisterLocalVariable(resolver, GetIntegerType(32, true), statement->startValue, statement->iteratorName, false, resolver->file, statement->inputState);

	result = ResolveStatement(resolver, statement->body) && result;

	ResolverPopScope(resolver);

	return result;
}

static bool ResolveBreak(Resolver* resolver, AstBreak* statement)
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
		SnekError(resolver->context, statement->inputState, ERROR_CODE_INVALID_BREAK, "No loop structure to break out of");
		return false;
	}
}

static bool ResolveContinue(Resolver* resolver, AstContinue* statement)
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
		SnekError(resolver->context, statement->inputState, ERROR_CODE_INVALID_CONTINUE, "No loop structure to continue");
		return false;
	}
}

static bool ResolveReturn(Resolver* resolver, AstReturn* statement)
{
	if (statement->value)
	{
		if (ResolveExpression(resolver, statement->value))
		{
			AstFunction* currentFunction = resolver->currentFunction;

			if (CanConvertImplicit(statement->value->type, currentFunction->returnType->typeID, IsLiteral(statement->value)))
			{
				return true;
			}
			else
			{
				SnekError(resolver->context, statement->inputState, ERROR_CODE_RETURN_WRONG_TYPE, "Can't return value of type %s from function with return type %s", GetTypeString(statement->value->type), GetTypeString(currentFunction->returnType->typeID));
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

static bool ResolveFree(Resolver* resolver, AstFree* statement)
{
	bool result = true;

	for (int i = 0; i < statement->values.size; i++)
	{
		result = ResolveExpression(resolver, statement->values[i]) && result;
	}

	return result;
}

static bool ResolveStatement(Resolver* resolver, AstStatement* statement)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = statement;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	switch (statement->statementKind)
	{
	case STATEMENT_KIND_NO_OP:
		return true;
	case STATEMENT_KIND_COMPOUND:
		return ResolveCompoundStatement(resolver, (AstCompoundStatement*)statement);
	case STATEMENT_KIND_EXPR:
		return ResolveExprStatement(resolver, (AstExprStatement*)statement);
	case STATEMENT_KIND_VAR_DECL:
		return ResolveVarDeclStatement(resolver, (AstVarDeclStatement*)statement);
	case STATEMENT_KIND_IF:
		return ResolveIfStatement(resolver, (AstIfStatement*)statement);
	case STATEMENT_KIND_WHILE:
		return ResolveWhileLoop(resolver, (AstWhileLoop*)statement);
	case STATEMENT_KIND_FOR:
		return ResolveForLoop(resolver, (AstForLoop*)statement);
	case STATEMENT_KIND_BREAK:
		return ResolveBreak(resolver, (AstBreak*)statement);
	case STATEMENT_KIND_CONTINUE:
		return ResolveContinue(resolver, (AstContinue*)statement);
	case STATEMENT_KIND_RETURN:
		return ResolveReturn(resolver, (AstReturn*)statement);
	case STATEMENT_KIND_FREE:
		return ResolveFree(resolver, (AstFree*)statement);

	default:
		SnekAssert(false);
		return false;
	}
}

static AstVisibility GetVisibilityFromFlags(AstDeclFlags flags)
{
	if (flags & DECL_FLAG_VISIBILITY_PUBLIC)
		return VISIBILITY_PUBLIC;
	else if (flags & DECL_FLAG_VISIBILITY_PRIVATE)
		return VISIBILITY_PRIVATE;
	else
		return VISIBILITY_PRIVATE;
}

static bool CheckEntrypointDecl(Resolver* resolver, AstFunction* function)
{
	if (function->visibility != VISIBILITY_PUBLIC)
	{
		SnekError(resolver->context, function->inputState, ERROR_CODE_ENTRY_POINT_DECLARATION, "Entry point must be public");
		return false;
	}
	if (function->returnType->typeKind != TYPE_KIND_VOID)
	{
		SnekError(resolver->context, function->inputState, ERROR_CODE_ENTRY_POINT_DECLARATION, "Entry point must return void");
		return false;
	}
	if (function->paramTypes.size > 0)
	{
		SnekError(resolver->context, function->inputState, ERROR_CODE_ENTRY_POINT_DECLARATION, "Entry point must not have parameters");
		return false;
	}
	return true;
}

static bool ResolveFunctionHeader(Resolver* resolver, AstFunction* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	decl->mangledName = MangleFunctionName(decl);
	decl->visibility = GetVisibilityFromFlags(decl->flags);

	decl->isEntryPoint = strcmp(decl->name, "Main") == 0;
	if (decl->isEntryPoint)
	{
		result = CheckEntrypointDecl(resolver, decl) && result;
	}

	if (decl->isGeneric)
	{
		// Don't resolve types
	}
	else
	{
		result = ResolveType(resolver, decl->returnType) && result;
		for (int i = 0; i < decl->paramTypes.size; i++)
		{
			result = ResolveType(resolver, decl->paramTypes[i]) && result;
		}

		int numParams = decl->paramTypes.size;
		TypeID returnType = decl->returnType->typeID;
		TypeID* paramTypes = new TypeID[numParams];
		for (int i = 0; i < numParams; i++)
		{
			paramTypes[i] = decl->paramTypes[i]->typeID;
		}
		decl->type = GetFunctionType(returnType, numParams, paramTypes, decl->varArgs, false, decl);
	}

	return result;
}

static bool ResolveFunction(Resolver* resolver, AstFunction* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	if (decl->flags & DECL_FLAG_EXTERN)
	{
		if (decl->body)
		{
			SnekError(resolver->context, decl->inputState, ERROR_CODE_FUNCTION_SYNTAX, "Extern function '%s' cannot have a body", decl->name);
			return false;
		}
	}

	if (decl->isGeneric)
	{
		// Don't resolve types
	}
	else
	{
		if (decl->body)
		{
			AstFunction* lastFunction = resolver->currentFunction;
			resolver->currentFunction = decl;

			ResolverPushScope(resolver, decl->name);

			decl->paramVariables = CreateList<AstVariable*>();
			decl->paramVariables.resize(decl->paramTypes.size);

			for (int i = 0; i < decl->paramTypes.size; i++)
			{
				AstVariable* variable = RegisterLocalVariable(resolver, decl->paramTypes[i]->typeID, NULL, decl->paramNames[i], false, resolver->file, decl->paramTypes[i]->inputState);
				decl->paramVariables[i] = variable;
			}

			result = ResolveStatement(resolver, decl->body) && result;

			ResolverPopScope(resolver);

			resolver->currentFunction = lastFunction;
		}
	}

	return result;
}

static bool ResolveStructHeader(Resolver* resolver, AstStruct* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	decl->mangledName = _strdup(decl->name);
	decl->visibility = GetVisibilityFromFlags(decl->flags);
	decl->type = GetStructType(decl->name, decl);

	return true;
}

static bool ResolveStruct(Resolver* resolver, AstStruct* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	decl->type->structType.hasBody = decl->hasBody;

	if (decl->hasBody)
	{
		int numFields = decl->fields.size;
		TypeID* fieldTypes = new TypeID[numFields];
		const char** fieldNames = new const char* [numFields];
		for (int i = 0; i < numFields; i++)
		{
			AstStructField* field = &decl->fields[i];
			if (ResolveType(resolver, field->type))
			{
				fieldTypes[i] = decl->fields[i].type->typeID;
			}
			else
			{
				result = false;
			}
			fieldNames[i] = decl->fields[i].name;
		}
		decl->type->structType.numFields = numFields;
		decl->type->structType.fieldTypes = fieldTypes;
		decl->type->structType.fieldNames = fieldNames;
	}
	else
	{
		decl->type->structType.numFields = 0;
		decl->type->structType.fieldTypes = NULL;
		decl->type->structType.fieldNames = NULL;
	}

	return result;
}

static bool ResolveClassHeader(Resolver* resolver, AstClass* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	decl->mangledName = _strdup(decl->name);
	decl->visibility = GetVisibilityFromFlags(decl->flags);
	decl->type = GetClassType(decl->name, decl);

	return true;
}

static bool ResolveClassMethodHeader(Resolver* resolver, AstFunction* method, AstClass* decl)
{
	AstElement* lastElement = resolver->currentElement;
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
	method->type = GetFunctionType(returnType, numParams, paramTypes, method->varArgs, true, method);

	return result;
}

static bool ResolveClassConstructorHeader(Resolver* resolver, AstFunction* constructor, AstClass* decl)
{
	AstElement* lastElement = resolver->currentElement;
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
	TypeID returnType = constructor->instanceType;
	TypeID* paramTypes = new TypeID[numParams];
	for (int i = 0; i < numParams; i++)
	{
		paramTypes[i] = constructor->paramTypes[i]->typeID;
	}

	constructor->instanceType = decl->type;
	constructor->type = GetFunctionType(returnType, numParams, paramTypes, constructor->varArgs, true, constructor);

	return result;
}

static bool ResolveClassProcedureHeaders(Resolver* resolver, AstClass* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	int numMethods = decl->methods.size;
	for (int i = 0; i < numMethods; i++)
	{
		AstFunction* method = decl->methods[i];
		result = ResolveClassMethodHeader(resolver, method, decl) && result;
	}

	if (decl->constructor)
	{
		result = ResolveClassConstructorHeader(resolver, decl->constructor, decl) && result;
	}

	return result;
}

static bool ResolveClass(Resolver* resolver, AstClass* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	int numFields = decl->fields.size;
	TypeID* fieldTypes = new TypeID[numFields];
	const char** fieldNames = new const char* [numFields];
	for (int i = 0; i < numFields; i++)
	{
		AstClassField* field = &decl->fields[i];
		if (ResolveType(resolver, field->type))
		{
			fieldTypes[i] = decl->fields[i].type->typeID;
		}
		else
		{
			result = false;
		}
		fieldNames[i] = decl->fields[i].name;
	}
	decl->type->classType.numFields = numFields;
	decl->type->classType.fieldTypes = fieldTypes;
	decl->type->classType.fieldNames = fieldNames;

	return result;
}

static bool ResolveClassMethod(Resolver* resolver, AstFunction* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	AstFunction* lastFunction = resolver->currentFunction;
	resolver->currentFunction = decl;

	ResolverPushScope(resolver, decl->name);

	decl->paramVariables = CreateList<AstVariable*>();
	decl->paramVariables.resize(decl->paramTypes.size);

	decl->instanceVariable = RegisterLocalVariable(resolver, decl->instanceType, NULL, "this", false, resolver->file, decl->inputState);
	for (int i = 0; i < decl->paramTypes.size; i++)
	{
		AstVariable* variable = RegisterLocalVariable(resolver, decl->paramTypes[i]->typeID, NULL, decl->paramNames[i], false, resolver->file, decl->paramTypes[i]->inputState);
		decl->paramVariables[i] = variable;
	}

	result = ResolveStatement(resolver, decl->body) && result;

	ResolverPopScope(resolver);

	resolver->currentFunction = lastFunction;

	return result;
}

static bool ResolveClassConstructor(Resolver* resolver, AstFunction* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	AstFunction* lastFunction = resolver->currentFunction;
	resolver->currentFunction = decl;

	ResolverPushScope(resolver, decl->name);

	decl->paramVariables = CreateList<AstVariable*>();
	decl->paramVariables.resize(decl->paramTypes.size);

	decl->instanceVariable = RegisterLocalVariable(resolver, decl->instanceType, NULL, "this", false, resolver->file, decl->inputState);
	for (int i = 0; i < decl->paramTypes.size; i++)
	{
		AstVariable* variable = RegisterLocalVariable(resolver, decl->paramTypes[i]->typeID, NULL, decl->paramNames[i], false, resolver->file, decl->inputState);
		decl->paramVariables[i] = variable;
	}

	result = ResolveStatement(resolver, decl->body) && result;

	ResolverPopScope(resolver);

	resolver->currentFunction = lastFunction;

	return result;
}

static bool ResolveClassProcedures(Resolver* resolver, AstClass* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	int numMethods = decl->methods.size;
	for (int i = 0; i < numMethods; i++)
	{
		AstFunction* method = decl->methods[i];
		result = ResolveClassMethod(resolver, method) && result;
	}

	if (decl->constructor)
	{
		result = ResolveClassConstructor(resolver, decl->constructor) && result;
	}

	return result;
}

static bool ResolveTypedefHeader(Resolver* resolver, AstTypedef* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	decl->visibility = GetVisibilityFromFlags(decl->flags);
	decl->type = GetAliasType(decl->name, decl);

	return true;
}

static bool ResolveTypedef(Resolver* resolver, AstTypedef* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	result = ResolveType(resolver, decl->alias) && result;
	decl->type->aliasType.alias = decl->alias->typeID;

	return result;
}

static bool ResolveEnumHeader(Resolver* resolver, AstEnum* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	decl->visibility = GetVisibilityFromFlags(decl->flags);
	decl->type = GetAliasType(decl->name, decl);
	decl->type->aliasType.alias = GetIntegerType(32, true);

	bool result = true;

	for (int i = 0; i < decl->values.size; i++)
	{
		decl->values[i].enumDecl = decl;
		if (decl->values[i].value)
		{
			if (!ResolveExpression(resolver, decl->values[i].value))
			{
				result = false;
			}
		}
	}

	return true;
}

static bool ResolveEnum(Resolver* resolver, AstEnum* decl)
{
	return true;
}

static bool ResolveExprdefHeader(Resolver* resolver, AstExprdef* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	decl->visibility = GetVisibilityFromFlags(decl->flags);

	return true;
}

static bool ResolveGlobalHeader(Resolver* resolver, AstGlobal* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	result = ResolveType(resolver, decl->type) && result;

	AstVisibility visibility = GetVisibilityFromFlags(decl->flags);
	decl->variable = RegisterGlobalVariable(resolver, decl->type->typeID, decl->value, decl->name, decl->flags & DECL_FLAG_CONSTANT, visibility, resolver->file, decl);

	return result;
}

static bool ResolveGlobal(Resolver* resolver, AstGlobal* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	if (decl->value)
	{
		if (ResolveExpression(resolver, decl->value))
		{
			if (!CanConvertImplicit(decl->value->type, decl->type->typeID, IsConstant(decl->value)))
			{
				SnekError(resolver->context, decl->inputState, ERROR_CODE_GLOBAL_TYPE_MISMATCH, "Can't initialize global variable '%s' of type %s with value of type %s", decl->name, GetTypeString(decl->type->typeID), GetTypeString(decl->value->type));
				result = false;
			}
			if (decl->flags & DECL_FLAG_EXTERN)
			{
				SnekError(resolver->context, decl->inputState, ERROR_CODE_GLOBAL_EXTERN_INITIALIZER, "Extern global variable '%s' cannot have an initializer", decl->name);
				result = false;
			}
			else if (!IsConstant(decl->value))
			{
				SnekError(resolver->context, decl->inputState, ERROR_CODE_GLOBAL_INITIALIZER_NONCONSTANT, "Initializer of global variable '%s' must be a constant value", decl->name);
				result = false;
			}
		}
		else
		{
			result = false;
		}
	}

	return result;
}

static AstFile* FindModuleByName(Resolver* resolver, const char* name)
{
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* module = resolver->context->asts[i];
		if (strcmp(module->name, name) == 0)
			return module;
	}
	return NULL;
}

static bool ResolveModuleDecl(Resolver* resolver, AstModuleDecl* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	AstModule* parent = resolver->globalNamespace;
	for (int i = 0; i < decl->namespaces.size; i++)
	{
		const char* name = decl->namespaces[i];
		AstModule* ns = FindModule(resolver, name, parent);
		if (!ns)
		{
			ns = CreateModule(resolver, name, parent);
		}
		parent = ns;
	}

	parent->files.add(decl->module);
	decl->ns = parent;

	return true;
}

static void AddModuleDependency(AstFile* file, AstModule* module, bool recursive)
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

static bool ResolveImport(Resolver* resolver, AstImport* decl)
{
	AstElement* lastElement = resolver->currentElement;
	resolver->currentElement = decl;
	defer _(nullptr, [=](...) { resolver->currentElement = lastElement; });

	bool result = true;

	for (int i = 0; i < decl->imports.size; i++)
	{
		List<char*>& names = decl->imports[i];
		AstModule* parent = NULL;
		for (int j = 0; j < names.size; j++)
		{
			const char* name = names[j];
			if (strcmp(name, "*") == 0)
			{
				if (j == names.size - 1)
				{
					for (int k = 0; k < parent->children.size; k++)
					{
						AddModuleDependency(resolver->file, parent->children[k], false);
					}
					break;
				}
				else
				{
					SnekError(resolver->context, decl->inputState, ERROR_CODE_IMPORT_SYNTAX, "For bulk imports, * must be at the end of the import declaration");
					result = false;
					break;
				}
			}
			else if (strcmp(name, "**") == 0)
			{
				if (j == names.size - 1)
				{
					AddModuleDependency(resolver->file, parent, true);
					break;
				}
				else
				{
					SnekError(resolver->context, decl->inputState, ERROR_CODE_IMPORT_SYNTAX, "For recursive bulk imports, ** must be at the end of the import declaration");
					result = false;
					break;
				}
			}
			else if (AstModule* ns = FindModule(resolver, name, parent))
			{
				if (j == names.size - 1)
				{
					AddModuleDependency(resolver->file, ns, false);
					break;
				}
				else
				{
					parent = ns;
				}
			}
			else
			{
				char module_name[64];
				module_name[0] = '\0';
				for (int k = 0; k <= j; k++)
				{
					strcat(module_name, names[k]);
					if (k < j)
						strcat(module_name, ".");
				}

				SnekError(resolver->context, decl->inputState, ERROR_CODE_UNKNOWN_MODULE, "Unknown module '%s'", module_name);
				result = false;
				parent = NULL;
				break;
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
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		resolver->file->module = resolver->globalNamespace;

		if (ast->moduleDecl)
		{
			result = ResolveModuleDecl(resolver, ast->moduleDecl) && result;
			resolver->file->module = ast->moduleDecl->ns;
		}
		if (ast->namespaceDecl)
		{
			//result = ResolveNamespaceDecl(resolver, ast->namespaceDecl) && result;
			resolver->file->nameSpace = ast->namespaceDecl->name;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->imports.size; j++)
		{
			result = ResolveImport(resolver, ast->imports[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->globals.size; j++)
		{
			bool isPrimitiveConstant = ast->globals[j]->flags & DECL_FLAG_CONSTANT && IsPrimitiveType(ast->globals[j]->type->typeKind);
			if (isPrimitiveConstant)
				result = ResolveGlobalHeader(resolver, ast->globals[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->enums.size; j++)
		{
			result = ResolveEnumHeader(resolver, ast->enums[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->structs.size; j++)
		{
			result = ResolveStructHeader(resolver, ast->structs[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->classes.size; j++)
		{
			result = ResolveClassHeader(resolver, ast->classes[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->typedefs.size; j++)
		{
			result = ResolveTypedefHeader(resolver, ast->typedefs[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->exprdefs.size; j++)
		{
			result = ResolveExprdefHeader(resolver, ast->exprdefs[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
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
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->globals.size; j++)
		{
			bool isPrimitiveConstant = ast->globals[j]->flags & DECL_FLAG_CONSTANT && IsPrimitiveType(ast->globals[j]->type->typeKind);
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
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->globals.size; j++)
		{
			bool isPrimitiveConstant = ast->globals[j]->flags & DECL_FLAG_CONSTANT && IsPrimitiveType(ast->globals[j]->type->typeKind);
			if (isPrimitiveConstant)
				result = ResolveGlobal(resolver, ast->globals[j]) && result;
		}
	}

	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->enums.size; j++)
		{
			result = ResolveEnum(resolver, ast->enums[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->structs.size; j++)
		{
			result = ResolveStruct(resolver, ast->structs[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->classes.size; j++)
		{
			result = ResolveClass(resolver, ast->classes[j]) && result;
		}
	}
	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->typedefs.size; j++)
		{
			result = ResolveTypedef(resolver, ast->typedefs[j]) && result;
		}
	}
	if (!result) // Return early if types have not been resolved
		return result;

	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
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
		AstFile* ast = resolver->context->asts[i];
		resolver->file = ast;
		for (int j = 0; j < ast->globals.size; j++)
		{
			bool isPrimitiveConstant = ast->globals[j]->flags & DECL_FLAG_CONSTANT && IsPrimitiveType(ast->globals[j]->type->typeKind);
			if (!isPrimitiveConstant)
				result = ResolveGlobal(resolver, ast->globals[j]) && result;
		}
	}

	return result;
}

bool ResolverRun(Resolver* resolver)
{
	bool result = true;

	InitTypeData();

	resolver->file = NULL;

	resolver->currentFunction = NULL;
	resolver->currentElement = NULL;
	//resolver->currentStatement = NULL;
	//resolver->currentExpression = NULL;

	resolver->scope = NULL;

	result = ResolveModuleHeaders(resolver) && result;
	if (result) result = ResolveModules(resolver) && result;

	return result;
}

Scope* ResolverPushScope(Resolver* resolver, const char* name)
{
	Scope* scope = new Scope();
	scope->parent = resolver->scope;
	scope->name = name;
	resolver->scope = scope;
	return scope;
}

void ResolverPopScope(Resolver* resolver)
{
	Scope* scope = resolver->scope;
	resolver->scope = scope->parent;
	delete scope;
}
