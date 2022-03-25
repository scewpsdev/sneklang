#include "mangle.h"

#include "semantics/Resolver.h"
#include "ast/Declaration.h"
#include "ast/File.h"
#include "ast/Module.h"

#include "stringbuffer.h"

#include <string.h>


static void AppendModuleName(Resolver* resolver, AST::Module* module, StringBuffer& buffer)
{
	if (module->parent != resolver->globalNamespace)
	{
		AppendModuleName(resolver, module->parent, buffer);
		StringBufferAppend(buffer, "_");
	}
	StringBufferAppend(buffer, module->name);
}

char* MangleFunctionName(Resolver* resolver, AST::Function* function)
{
	bool isExtern = HasFlag(function->flags, AST::DeclarationFlags::Extern);

	if (strcmp(function->name, ENTRY_POINT_NAME) == 0)
	{
		return _strdup("main");
	}
	else if (isExtern)
	{
		return _strdup(function->name);
	}
	else
	{
		StringBuffer result = CreateStringBuffer(4);
		AST::Module* module = function->file->module;
		AppendModuleName(resolver, module, result);
		StringBufferAppend(result, '_');
		StringBufferAppend(result, function->name);

		if (function->isGenericInstance)
		{
			for (int i = 0; i < function->genericTypeArguments.size; i++)
			{
				const char* typeString = GetTypeString(function->genericTypeArguments[i]);
				StringBufferAppend(result, '_');
				StringBufferAppend(result, typeString);
			}
		}

		return result.buffer;
	}
}
