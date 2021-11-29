#pragma once

#include "list.h"
#include "type.h"


struct SkContext;
struct AstModule;
struct AstFile;
struct AstElement;
struct AstStatement;
struct AstExpression;
struct AstVariable;
struct AstFunction;

struct Scope
{
	Scope* parent = NULL;
	const char* name;

	AstStatement* branchDst;

	List<AstVariable*> localVariables;
};

struct Resolver
{
	SkContext* context;

	AstModule* globalNamespace;

	AstFile* file;
	AstFunction* currentFunction;
	AstElement* currentElement;
	//AstStatement* currentStatement;
	//AstExpression* currentExpression;

	Scope* scope = NULL;
};


Resolver* CreateResolver(SkContext* context);
void DestroyResolver(Resolver* resolver);

bool ResolverRun(Resolver* resolver);

Scope* ResolverPushScope(Resolver* resolver, const char* name);
void ResolverPopScope(Resolver* resolver);
