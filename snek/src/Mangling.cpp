#include "Mangling.h"

#include "semantics/Resolver.h"
#include "ast/Declaration.h"
#include "ast/File.h"
#include "ast/Module.h"

#include "stringbuffer.h"

#include <string.h>


static void MangleType(TypeID type, StringBuffer& buffer)
{
	if (type->typeKind == AST::TypeKind::Struct && type->structType.declaration && type->structType.declaration->isGenericInstance)
	{
		char* mangledName = MangleStructName(type->structType.declaration);
		StringBufferAppend(buffer, mangledName);
		delete mangledName;
	}
	else
	{
		StringBufferAppend(buffer, GetTypeString(type));
	}
}

static void AppendModuleName(AST::Module* module, StringBuffer& buffer)
{
	if (module->parent->parent) // If parent is not the global module
	{
		AppendModuleName(module->parent, buffer);
		StringBufferAppend(buffer, "_");
	}
	StringBufferAppend(buffer, module->name);
}

char* MangleFunctionName(AST::Function* function)
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
		AppendModuleName(module, result);
		StringBufferAppend(result, '_');
		StringBufferAppend(result, function->name);

		if (function->isGenericInstance)
		{
			for (int i = 0; i < function->genericTypeArguments.size; i++)
			{
				StringBufferAppend(result, '_');
				MangleType(function->genericTypeArguments[i], result);
			}
		}

		return result.buffer;
	}
}

char* MangleStructName(AST::Struct* str)
{
	StringBuffer result = CreateStringBuffer(4);
	StringBufferAppend(result, str->name);

	if (str->isGenericInstance)
	{
		for (int i = 0; i < str->genericTypeArguments.size; i++)
		{
			StringBufferAppend(result, '_');
			MangleType(str->genericTypeArguments[i], result);
		}
	}

	return result.buffer;
}
