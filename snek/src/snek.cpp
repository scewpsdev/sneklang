#include "snek.h"

#include "parser.h"
#include "resolver.h"

#include <string.h>


SkContext* CreateSnekContext(MessageCallback_t msgCallback)
{
	SkContext* context = new SkContext();

	context->sourceFiles = CreateList<SourceFile>();

	context->msgCallback = msgCallback;

	return context;
}

void DestroySnekContext(SkContext* context)
{
	delete context;
}

void SnekAddSource(SkContext* context, char* src, char* moduleName)
{
	SourceFile file;
	file.src = src;
	file.moduleName = moduleName;
	ListAdd(context->sourceFiles, file);
}

bool SnekRunParser(SkContext* context)
{
	context->asts = CreateList<AstModule*>(context->sourceFiles.size);
	ListResize(context->asts, context->sourceFiles.size);

	Parser* parser = CreateParser(context);
	ParserRun(parser);
	DestroyParser(parser);

	return true;
}

bool SnekRunResolver(SkContext* context)
{
	Resolver* resolver = CreateResolver(context);
	ResolverRun(resolver);
	DestroyResolver(resolver);

	return true;
}
