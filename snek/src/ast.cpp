#include "ast.h"

#include "log.h"

#include <stdlib.h>


AstFile* CreateAst(char* name, int moduleID, SourceFile* sourceFile)
{
	AstFile* ast = new AstFile();

	ast->name = name;
	ast->moduleID = moduleID;
	ast->sourceFile = sourceFile;

	ast->blocks = CreateList<AstElementBlock*>();
	ast->index = 0;

	AstElementBlock* firstBlock = (AstElementBlock*)malloc(sizeof(AstElementBlock));
	memset(firstBlock, 0, sizeof(AstElementBlock));
	ast->blocks.add(firstBlock);

	ast->moduleDecl = NULL;

	ast->functions = CreateList<AstFunction*>();
	ast->structs = CreateList<AstStruct*>();
	ast->classes = CreateList<AstClass*>();
	ast->typedefs = CreateList<AstTypedef*>();
	ast->enums = CreateList<AstEnum*>();
	ast->exprdefs = CreateList<AstExprdef*>();
	ast->globals = CreateList<AstGlobal*>();
	ast->imports = CreateList<AstImport*>();

	ast->dependencies = CreateList<AstModule*>();

	return ast;
}

static AstElement* CreateAstElement(AstFile* module, const InputState& inputState, int size)
{
	int index = module->index;
	int relativeIdx = index % ELEMENT_BLOCK_SIZE;
	AstElementBlock* block = module->blocks[module->blocks.size - 1];

	if (relativeIdx + size >= ELEMENT_BLOCK_SIZE)
	{
		AstElementBlock* newBlock = (AstElementBlock*)malloc(sizeof(AstElementBlock));
		memset(newBlock, 0, sizeof(AstElementBlock));
		module->blocks.add(newBlock);
		block = newBlock;

		index = ((index + size) / ELEMENT_BLOCK_SIZE) * ELEMENT_BLOCK_SIZE;
		relativeIdx = 0;
	}

	AstElement* element = (AstElement*)&block->elements[relativeIdx];
	element->inputState = inputState;
	element->module = module;
	module->index = index + size;

	return element;
}

AstType* CreateAstType(AstFile* module, const InputState& inputState, AstTypeKind typeKind)
{
	AstType* type = (AstType*)CreateAstElement(module, inputState, sizeof(AstTypeStorage));
	type->typeKind = typeKind;
	return type;
}

AstExpression* CreateAstExpression(AstFile* module, const InputState& inputState, AstExpressionKind exprKind)
{
	AstExpression* expression = (AstExpression*)CreateAstElement(module, inputState, sizeof(AstExpressionStorage));
	expression->exprKind = exprKind;
	return expression;
}

AstStatement* CreateAstStatement(AstFile* module, const InputState& inputState, AstStatementKind statementKind)
{
	AstStatement* statement = (AstStatement*)CreateAstElement(module, inputState, sizeof(AstStatementStorage));
	statement->statementKind = statementKind;
	return statement;
}

AstDeclaration* CreateAstDeclaration(AstFile* module, const InputState& inputState, AstDeclarationKind declKind)
{
	AstDeclaration* decl = (AstDeclaration*)CreateAstElement(module, inputState, sizeof(AstDeclarationStorage));
	decl->declKind = declKind;
	return decl;
}

AstVariable* CreateAstVariable(AstFile* file, const InputState& inputState)
{
	AstVariable* var = (AstVariable*)CreateAstElement(file, inputState, sizeof(AstVariable));
	return var;
}

AstType* CopyType(AstType* type, AstFile* module)
{
	AstType* clone = CreateAstType(module, type->inputState, type->typeKind);
	*((AstTypeStorage*)clone) = *((AstTypeStorage*)type);
	clone->module = module;

	switch (type->typeKind)
	{
	case TYPE_KIND_VOID:
		break;
	case TYPE_KIND_INTEGER:
		break;
	case TYPE_KIND_FP:
		break;
	case TYPE_KIND_BOOL:
		break;
	case TYPE_KIND_NAMED_TYPE:
		break;
	case TYPE_KIND_STRUCT:
		SnekAssert(false);
		break;
	case TYPE_KIND_CLASS:
		SnekAssert(false);
		break;
	case TYPE_KIND_ALIAS:
		SnekAssert(false);
		break;
	case TYPE_KIND_POINTER:
	{
		AstPointerType* _clone = (AstPointerType*)clone;
		AstPointerType* _type = (AstPointerType*)type;
		_clone->elementType = CopyType(_type->elementType, module);
		break;
	}
	case TYPE_KIND_FUNCTION:
	{
		AstFunctionType* _clone = (AstFunctionType*)clone;
		AstFunctionType* _type = (AstFunctionType*)type;
		_clone->returnType = CopyType(_type->returnType, module);
		_clone->paramTypes = CreateList<AstType*>(_type->paramTypes.size);
		_clone->paramTypes.resize(_type->paramTypes.size);
		for (int i = 0; i < _type->paramTypes.size; i++)
		{
			_clone->paramTypes[i] = CopyType(_type->paramTypes[i], module);
		}
		break;
	}
	case TYPE_KIND_ARRAY:
	{
		AstArrayType* _clone = (AstArrayType*)clone;
		AstArrayType* _type = (AstArrayType*)type;
		_clone->elementType = CopyType(_type->elementType, module);
		if (_type->length)
			_clone->length = CopyExpression(_type->length, module);
		break;
	}
	case TYPE_KIND_STRING:
		break;
	}

	return clone;
}

AstExpression* CopyExpression(AstExpression* expr, AstFile* module)
{
	AstExpression* clone = CreateAstExpression(module, expr->inputState, expr->exprKind);
	*((AstExpressionStorage*)clone) = *((AstExpressionStorage*)expr);
	clone->module = module;

	switch (expr->exprKind)
	{
	case EXPR_KIND_INTEGER_LITERAL:
		break;
	case EXPR_KIND_FP_LITERAL:
		break;
	case EXPR_KIND_BOOL_LITERAL:
		break;
	case EXPR_KIND_CHARACTER_LITERAL:
		break;
	case EXPR_KIND_NULL_LITERAL:
		break;
	case EXPR_KIND_STRING_LITERAL:
		break;
	case EXPR_KIND_IDENTIFIER:
		break;
	case EXPR_KIND_COMPOUND:
	{
		AstCompoundExpression* _clone = (AstCompoundExpression*)clone;
		AstCompoundExpression* _expr = (AstCompoundExpression*)expr;
		_clone->value = CopyExpression(_expr->value, module);
		break;
	}

	case EXPR_KIND_FUNC_CALL:
	{
		AstFuncCall* _clone = (AstFuncCall*)clone;
		AstFuncCall* _expr = (AstFuncCall*)expr;
		_clone->calleeExpr = CopyExpression(_expr->calleeExpr, module);
		_clone->arguments = CreateList<AstExpression*>(_expr->arguments.size);
		_clone->arguments.resize(_expr->arguments.size);
		for (int i = 0; i < _expr->arguments.size; i++)
		{
			_clone->arguments[i] = CopyExpression(_expr->arguments[i], module);
		}
		break;
	}
	case EXPR_KIND_SUBSCRIPT_OPERATOR:
	{
		AstSubscriptOperator* _clone = (AstSubscriptOperator*)clone;
		AstSubscriptOperator* _expr = (AstSubscriptOperator*)expr;
		_clone->operand = CopyExpression(_expr->operand, module);
		_clone->arguments = CreateList<AstExpression*>(_expr->arguments.size);
		_clone->arguments.resize(_expr->arguments.size);
		for (int i = 0; i < _expr->arguments.size; i++)
		{
			_clone->arguments[i] = CopyExpression(_expr->arguments[i], module);
		}
		break;
	}
	case EXPR_KIND_DOT_OPERATOR:
	{
		AstDotOperator* _clone = (AstDotOperator*)clone;
		AstDotOperator* _expr = (AstDotOperator*)expr;
		_clone->operand = CopyExpression(_expr->operand, module);
		break;
	}
	case EXPR_KIND_CAST:
	{
		AstCast* _clone = (AstCast*)clone;
		AstCast* _expr = (AstCast*)expr;
		_clone->value = CopyExpression(_expr->value, module);
		_clone->castType = CopyType(_expr->castType, module);
		break;
	}
	case EXPR_KIND_MALLOC:
	{
		AstMalloc* _clone = (AstMalloc*)clone;
		AstMalloc* _expr = (AstMalloc*)expr;
		_clone->mallocType = CopyType(_expr->mallocType, module);
		if (_expr->count)
			_clone->count = CopyExpression(_expr->count, module);

		if (_expr->hasArguments)
		{
			_clone->arguments = CreateList<AstExpression*>(_expr->arguments.size);
			_clone->arguments.resize(_expr->arguments.size);
			for (int i = 0; i < _expr->arguments.size; i++)
			{
				_clone->arguments[i] = CopyExpression(_expr->arguments[i], module);
			}
		}
		break;
	}

	case EXPR_KIND_UNARY_OPERATOR:
	{
		AstUnaryOperator* _clone = (AstUnaryOperator*)clone;
		AstUnaryOperator* _expr = (AstUnaryOperator*)expr;
		_clone->operand = CopyExpression(_expr->operand, module);
		break;
	}
	case EXPR_KIND_BINARY_OPERATOR:
	{
		AstBinaryOperator* _clone = (AstBinaryOperator*)clone;
		AstBinaryOperator* _expr = (AstBinaryOperator*)expr;
		_clone->left = CopyExpression(_expr->left, module);
		_clone->right = CopyExpression(_expr->right, module);
		break;
	}
	case EXPR_KIND_TERNARY_OPERATOR:
	{
		AstTernaryOperator* _clone = (AstTernaryOperator*)clone;
		AstTernaryOperator* _expr = (AstTernaryOperator*)expr;
		_clone->condition = CopyExpression(_expr->condition, module);
		_clone->thenValue = CopyExpression(_expr->thenValue, module);
		_clone->elseValue = CopyExpression(_expr->elseValue, module);
		break;
	}

	default:
		SnekAssert(false);
		break;
	}

	return clone;
}

bool IsLiteral(AstExpression* expression)
{
	return
		expression->exprKind == EXPR_KIND_INTEGER_LITERAL ||
		expression->exprKind == EXPR_KIND_FP_LITERAL ||
		expression->exprKind == EXPR_KIND_CHARACTER_LITERAL ||
		expression->exprKind == EXPR_KIND_STRING_LITERAL ||
		expression->exprKind == EXPR_KIND_BOOL_LITERAL ||
		expression->exprKind == EXPR_KIND_NULL_LITERAL;
}

bool IsConstant(AstExpression* expr)
{
	switch (expr->exprKind)
	{
	case EXPR_KIND_INTEGER_LITERAL:
		return true;
	case EXPR_KIND_FP_LITERAL:
		return true;
	case EXPR_KIND_BOOL_LITERAL:
		return true;
	case EXPR_KIND_CHARACTER_LITERAL:
		return true;
	case EXPR_KIND_NULL_LITERAL:
		return true;
	case EXPR_KIND_STRING_LITERAL:
		return true;
	case EXPR_KIND_STRUCT_LITERAL:
	{
		AstStructLiteral* literal = (AstStructLiteral*)expr;
		for (int i = 0; i < literal->values.size; i++)
		{
			if (!IsConstant(literal->values[i]))
				return false;
		}
		return true;
	}
	case EXPR_KIND_IDENTIFIER:
	{
		AstIdentifier* identifier = (AstIdentifier*)expr;
		if (identifier->variable)
			return identifier->variable->isConstant;
		else if (identifier->function)
			return true;
		else if (identifier->exprdefValue)
			return false;
		else if (identifier->enumValue)
			return true;
		SnekAssert(false);
		return false;
	}
	case EXPR_KIND_COMPOUND:
		return IsConstant(((AstCompoundExpression*)expr)->value);

	case EXPR_KIND_FUNC_CALL:
		// TODO return true if calling pure function
		return false;
	case EXPR_KIND_SUBSCRIPT_OPERATOR:
		return false;
	case EXPR_KIND_DOT_OPERATOR:
		return false;
	case EXPR_KIND_CAST:
		return false;
	case EXPR_KIND_MALLOC:
		return false;

	case EXPR_KIND_UNARY_OPERATOR:
		return IsConstant(((AstUnaryOperator*)expr)->operand);
	case EXPR_KIND_BINARY_OPERATOR:
		return IsConstant(((AstBinaryOperator*)expr)->left) && IsConstant(((AstBinaryOperator*)expr)->right);
	case EXPR_KIND_TERNARY_OPERATOR:
		return IsConstant(((AstTernaryOperator*)expr)->condition) && IsConstant(((AstTernaryOperator*)expr)->thenValue) && IsConstant(((AstTernaryOperator*)expr)->elseValue);

	default:
		SnekAssert(false);
		return false;
	}
}
