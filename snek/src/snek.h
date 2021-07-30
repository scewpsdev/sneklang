#pragma once

#include "ast.h"
#include "log.h"
#include "list.h"


struct SourceFile
{
	char* src;
	char* moduleName;
};

struct SkContext
{
	List<SourceFile> sourceFiles;
	List<AstModule*> asts;

	MessageCallback_t msgCallback;
};


SkContext* CreateSnekContext(MessageCallback_t msgCallback);
void DestroySnekContext(SkContext* context);

void SnekAddSource(SkContext* context, char* src, char* moduleName);
bool SnekRunParser(SkContext* context);
bool SnekRunResolver(SkContext* context);
