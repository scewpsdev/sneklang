#include "Mangling.h"

#include "semantics/Resolver.h"
#include "ast/Declaration.h"
#include "ast/File.h"
#include "ast/Module.h"

#include "utils/Stringbuffer.h"
#include "utils/Log.h"
#include "utils/Hash.h"

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
		switch (type->typeKind)
		{
		case AST::TypeKind::Void:
			StringBufferAppend(buffer, 'v');
			break;
		case AST::TypeKind::Integer:
			StringBufferAppend(buffer, type->integerType.isSigned ? 'i' : 'u');
			StringBufferAppend(buffer, type->integerType.bitWidth);
			break;
		case AST::TypeKind::FloatingPoint:
			StringBufferAppend(buffer, 'f');
			StringBufferAppend(buffer, (int)type->fpType.precision);
			break;
		case AST::TypeKind::Boolean:
			StringBufferAppend(buffer, 'b');
			break;
		case AST::TypeKind::Struct:
			StringBufferAppend(buffer, 'x');
			StringBufferAppend(buffer, (unsigned long)hash(type->structType.name));
			break;
		case AST::TypeKind::Class:
			StringBufferAppend(buffer, 'X');
			StringBufferAppend(buffer, (unsigned long)hash(type->classType.name));
			break;
		case AST::TypeKind::Alias:
			MangleType(type->aliasType.alias, buffer);
			break;
		case AST::TypeKind::Pointer:
			StringBufferAppend(buffer, 'p');
			MangleType(type->pointerType.elementType, buffer);
			break;
		case AST::TypeKind::Function:
			StringBufferAppend(buffer, 'f');
			MangleType(type->functionType.returnType, buffer);
			StringBufferAppend(buffer, type->functionType.numParams);
			for (int i = 0; i < type->functionType.numParams; i++)
				MangleType(type->functionType.paramTypes[i], buffer);
			break;
		case AST::TypeKind::Array:
			StringBufferAppend(buffer, 'a');
			MangleType(type->arrayType.elementType, buffer);
			break;
		case AST::TypeKind::String:
			StringBufferAppend(buffer, 's');
			break;
		default:
			SnekAssert(false);
			break;
		}
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

	if (function->isEntryPoint)
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
		if (module->parent)
		{
			AppendModuleName(module, result);
			StringBufferAppend(result, "__");
		}
		StringBufferAppend(result, function->name);

		if (function->paramTypes.size > 0)
		{
			StringBufferAppend(result, '_');
			StringBufferAppend(result, function->paramTypes.size);
			for (int i = 0; i < function->paramTypes.size; i++)
			{
				MangleType(function->paramTypes[i]->typeID, result);
			}
		}

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
	StringBuffer buffer = CreateStringBuffer(4);

	StringBufferAppend(buffer, str->name);

	if (str && str->isGenericInstance)
	{
		for (int i = 0; i < str->genericTypeArguments.size; i++)
		{
			StringBufferAppend(buffer, '_');
			MangleType(str->genericTypeArguments[i], buffer);
		}
	}

	return buffer.buffer;
}
