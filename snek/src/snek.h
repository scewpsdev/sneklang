#pragma once

#include "utils/Log.h"
#include "utils/List.h"


namespace AST
{
	struct File;
}

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
	List<AST::File*> asts;

	MessageCallback_t msgCallback;
};


SkContext* CreateSnekContext(MessageCallback_t msgCallback);
void DestroySnekContext(SkContext* context);

void SnekAddSource(SkContext* context, char* src, char* path, char* moduleName, char* filename, char* directory);
void SnekAddLinkerFile(SkContext* context, const char* path);
bool SnekRunParser(SkContext* context);
bool SnekRunResolver(SkContext* context);
