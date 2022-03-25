#include "snek.h"

#include "parser/Parser.h"
#include "semantics/Resolver.h"

#include <string.h>


SkContext* CreateSnekContext(MessageCallback_t msgCallback)
{
	SkContext* context = new SkContext();

	context->sourceFiles = CreateList<SourceFile>();
	context->linkerFiles = CreateList<LinkerFile>();

	context->msgCallback = msgCallback;

	return context;
}

void DestroySnekContext(SkContext* context)
{
	DestroyList(context->sourceFiles);
	DestroyList(context->linkerFiles);

	delete context;
}

void SnekAddSource(SkContext* context, char* src, char* path, char* moduleName, char* filename, char* directory)
{
	SourceFile file = {};
	file.src = src;
	file.path = path;
	file.moduleName = moduleName;
	file.filename = filename;
	file.directory = directory;
	context->sourceFiles.add(file);
}

void SnekAddLinkerFile(SkContext* context, const char* path)
{
	LinkerFile file = {};
	file.path = path;
	context->linkerFiles.add(file);
}

bool SnekRunParser(SkContext* context)
{
	context->asts = CreateList<AST::File*>(context->sourceFiles.size);
	context->asts.resize(context->sourceFiles.size);

	Parser* parser = CreateParser(context);
	bool result = ParserRun(parser);
	DestroyParser(parser);

	return result;
}

bool SnekRunResolver(SkContext* context)
{
	Resolver* resolver = new Resolver(context);
	bool result = resolver->run();
	//delete resolver;
	return result;
}
