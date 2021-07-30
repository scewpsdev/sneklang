#pragma once

#include "list.h"


struct SkContext;
struct AstExpression;

struct Resolver
{
	SkContext* context;

	List<AstExpression*> unresolvedExpressions;
};


Resolver* CreateResolver(SkContext* context);
void DestroyResolver(Resolver* resolver);

void ResolverRun(Resolver* resolver);
