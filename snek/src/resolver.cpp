#include "resolver.h"

#include "snek.h"
#include "ast.h"


Resolver* CreateResolver(SkContext* context)
{
	Resolver* resolver = new Resolver;

	resolver->context = context;

	return resolver;
}

void DestroyResolver(Resolver* resolver)
{
	delete resolver;
}

static bool ResolveIntegerLiteral(Resolver* resolver, AstIntegerLiteral* expr)
{
	return true;
}

static bool ResolveFPLiteral(Resolver* resolver, AstFPLiteral* expr)
{
	return true;
}

static bool ResolveBoolLiteral(Resolver* resolver, AstBoolLiteral* expr)
{
	return true;
}

static bool ResolveBinaryOperation(Resolver* resolver, AstBinaryOperator* expr)
{
	return true;
}

static bool ResolveCharacterLiteral(Resolver* resolver, AstCharacterLiteral* expr)
{
	return true;
}

static bool ResolveNullLiteral(Resolver* resolver, AstNullLiteral* expr)
{
	return true;
}

static bool ResolveIdentifier(Resolver* resolver, AstIdentifier* expr)
{
	// TODO find variable or function
	return true;
}

static bool ResolveCompoundExpression(Resolver* resolver, AstCompoundExpression* expr)
{
	return true;
}

static bool ResolveFuncCall(Resolver* resolver, AstFuncCall* expr)
{
	return true;
}

static bool ResolveSubscriptOperator(Resolver* resolver, AstSubscriptOperator* expr)
{
	return true;
}

static bool ResolveDotOperator(Resolver* resolver, AstDotOperator* expr)
{
	return true;
}

static bool ResolveCast(Resolver* resolver, AstCast* expr)
{
	return true;
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
