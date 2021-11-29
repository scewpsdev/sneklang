#pragma once

#include "ast.h"
#include "log.h"
#include "list.h"


struct SourceFile
{
	char* src;
	char* path;
	char* moduleName;
	char* filename;
	char* directory;
};

struct LinkerFile
{
	const char* path;
};

struct SkContext
{
	List<SourceFile> sourceFiles;
	List<LinkerFile> linkerFiles;
	List<AstFile*> asts;

	MessageCallback_t msgCallback;
};


SkContext* CreateSnekContext(MessageCallback_t msgCallback);
void DestroySnekContext(SkContext* context);

void SnekAddSource(SkContext* context, char* src, char* path, char* moduleName, char* filename, char* directory);
void SnekAddLinkerFile(SkContext* context, const char* path);
bool SnekRunParser(SkContext* context);
bool SnekRunResolver(SkContext* context);
