#include "resolver.h"

#include "snek.h"
#include "ast.h"
#include "variable.h"
#include "types.h"


Resolver* CreateResolver(SkContext* context)
{
	Resolver* resolver = new Resolver;

	resolver->context = context;

	InitTypeData(resolver);

	return resolver;
}

void DestroyResolver(Resolver* resolver)
{
	delete resolver;
}

static bool ResolveType(Resolver* resolver, AstType* type)
{
	return true;
}

static bool ResolveIntegerLiteral(Resolver* resolver, AstIntegerLiteral* expr)
{
	expr->type = GetIntegerType(resolver, 0);
	expr->lvalue = false;
	return true;
}

static bool ResolveFPLiteral(Resolver* resolver, AstFPLiteral* expr)
{
	expr->type = GetFPType(resolver, 0);
	expr->lvalue = false;
	return true;
}

static bool ResolveBoolLiteral(Resolver* resolver, AstBoolLiteral* expr)
{
	expr->type = GetBoolType(resolver);
	expr->lvalue = false;
	return true;
}

static bool ResolveCharacterLiteral(Resolver* resolver, AstCharacterLiteral* expr)
{
	expr->type = GetIntegerType(resolver, 8);
	expr->lvalue = false;
	return true;
}

static bool ResolveNullLiteral(Resolver* resolver, AstNullLiteral* expr)
{
	expr->type = GetIntegerType(resolver, 32);
	expr->lvalue = false;
	return true;
}

static bool ResolveIdentifier(Resolver* resolver, AstIdentifier* expr)
{
	if (AstVariable* variable = FindVariable(resolver, expr->name))
	{
		expr->type = variable->type;
		expr->lvalue = true;
		return true;
	}
	if (AstFunction* function = FindFunction(resolver, expr->name))
	{
		expr->type = function->type;
		expr->lvalue = true;
		return false;
	}
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

static bool ResolveFuncCall(Resolver* resolver, AstFuncCall* expr)
{
	if (expr->exprKind == EXPR_KIND_IDENTIFIER)
	{
		if (AstFunction* function = FindFunction(resolver, ((AstIdentifier*)expr)->name))
		{
			expr->func = function;
			expr->type = function->returnType->typeID;
			expr->lvalue = false;
			return true;
		}
	}

	if (ResolveExpression(resolver, expr->calleeExpr))
	{
		if (expr->calleeExpr->type->typeKind == TYPE_KIND_FUNCTION)
		{
			expr->type = expr->calleeExpr->type->functionType.returnType;
			expr->lvalue = false;
			return true;
		}
	}

	return false;
}

static bool ResolveSubscriptOperator(Resolver* resolver, AstSubscriptOperator* expr)
{
	if (ResolveExpression(resolver, expr->operand))
	{
		if (expr->operand->type->typeKind == TYPE_KIND_POINTER)
		{
			expr->type = expr->operand->type->pointerType.elementType;
			expr->lvalue = true;
			return true;
		}
	}
	return false;
}

static bool ResolveDotOperator(Resolver* resolver, AstDotOperator* expr)
{
	if (ResolveExpression(resolver, expr->operand))
	{
		if (expr->operand->type->typeKind == TYPE_KIND_STRUCT)
		{
			for (int i = 0; i < expr->operand->type->structType.numMembers; i++)
			{
				TypeID memberType = expr->operand->type->structType.memberTypes[i];
				const char* memberName = expr->operand->type->structType.memberNames[i];
				if (strcmp(memberName, expr->name) == 0)
				{
					expr->type = memberType;
					expr->lvalue = true;
					return true;
				}
			}
		}
	}
	return false;
}

static bool ResolveCast(Resolver* resolver, AstCast* expr)
{
	if (ResolveType(resolver, expr->castType))
	{
		expr->type = expr->castType->typeID;
		expr->lvalue = false;
		return true;
	}
	return false;
}

static bool ResolveBinaryOperation(Resolver* resolver, AstBinaryOperator* expr)
{
	if (ResolveExpression(resolver, expr->left) && ResolveExpression(resolver, expr->right))
	{
		expr->type = expr->left->type;
		expr->lvalue = false;
		return true;
	}
	return false;
}

static bool ResolveExpression(Resolver* resolver, AstExpression* expr)
{
	bool resolved = false;

	switch (expr->exprKind)
	{
	case EXPR_KIND_INTEGER_LITERAL:
		resolved = ResolveIntegerLiteral(resolver, (AstIntegerLiteral*)expr);
		break;
	case EXPR_KIND_FP_LITERAL:
		resolved = ResolveFPLiteral(resolver, (AstFPLiteral*)expr);
		break;
	case EXPR_KIND_BOOL_LITERAL:
		resolved = ResolveBoolLiteral(resolver, (AstBoolLiteral*)expr);
		break;
	case EXPR_KIND_CHARACTER_LITERAL:
		resolved = ResolveCharacterLiteral(resolver, (AstCharacterLiteral*)expr);
		break;
	case EXPR_KIND_NULL_LITERAL:
		resolved = ResolveNullLiteral(resolver, (AstNullLiteral*)expr);
		break;
	case EXPR_KIND_IDENTIFIER:
		resolved = ResolveIdentifier(resolver, (AstIdentifier*)expr);
		break;
	case EXPR_KIND_COMPOUND:
		resolved = ResolveCompoundExpression(resolver, (AstCompoundExpression*)expr);
		break;

	case EXPR_KIND_FUNC_CALL:
		resolved = ResolveFuncCall(resolver, (AstFuncCall*)expr);
		break;
	case EXPR_KIND_SUBSCRIPT_OPERATOR:
		resolved = ResolveSubscriptOperator(resolver, (AstSubscriptOperator*)expr);
		break;
	case EXPR_KIND_DOT_OPERATOR:
		resolved = ResolveDotOperator(resolver, (AstDotOperator*)expr);
		break;
	case EXPR_KIND_CAST:
		resolved = ResolveCast(resolver, (AstCast*)expr);
		break;

	case EXPR_KIND_BINARY_OPERATOR:
		resolved = ResolveBinaryOperation(resolver, (AstBinaryOperator*)expr);
		break;

	default:
		SnekAssert(false, "");
		break;
	}

	if (!resolved)
		ListAdd(resolver->unresolvedExpressions, expr);

	expr->resolved = resolved;

	return resolved;
}

static void ResolveFunc(Resolver* resolver, AstFunction* decl)
{
	decl->mangledName = _strdup(decl->name);
}

static void ResolveDeclaration(Resolver* resolver, AstDeclaration* decl)
{
	switch (decl->declKind)
	{
	case DECL_KIND_FUNC:
		ResolveFunc(resolver, (AstFunction*)decl);
		break;

	default:
		SnekAssert(false, "");
		break;
	}
}

static void ResolveModule(Resolver* resolver, AstModule* ast)
{
	for (int i = 0; i < ast->declarations.size; i++)
	{
		ResolveDeclaration(resolver, ast->declarations[i]);
	}
}

void ResolverRun(Resolver* resolver)
{
	// TODO multithreading

	for (int i = 0; i < resolver->context->asts.size; i++)
	{
		AstModule* ast = resolver->context->asts[i];
		ResolveModule(resolver, ast);

		for (int j = 0; j < resolver->unresolvedExpressions.size; j++)
		{
			SnekAssert(false, "");
			if (!ResolveExpression(resolver, resolver->unresolvedExpressions[j]))
			{
				// TODO ERROR
				SnekAssert(false, "");
			}
		}
	}
}
