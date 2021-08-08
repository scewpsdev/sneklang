#pragma once

#include "list.h"
#include "types.h"


struct SkContext;
struct AstExpression;

struct Resolver
{
	SkContext* context;

	TypeDataStorage types;

	List<AstExpression*> unresolvedExpressions;
};


Resolver* CreateResolver(SkContext* context);
void DestroyResolver(Resolver* resolver);

void ResolverRun(Resolver* resolver);
