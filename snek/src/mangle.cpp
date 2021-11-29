#include "mangle.h"

#include <string.h>


#define ENTRY_POINT "Main"


char* MangleFunctionName(AstFunction* function)
{
	bool isExtern = function->flags & DECL_FLAG_EXTERN;

	if (strcmp(function->name, ENTRY_POINT) == 0)
	{
		return _strdup("main");
	}
	else if (function->module->moduleDecl && !isExtern)
	{
		size_t resultLen = strlen(function->name);
		for (int i = 0; i < function->module->moduleDecl->namespaces.size; i++)
			resultLen += strlen("__") + strlen(function->module->moduleDecl->namespaces[i]);

		char* result = new char[resultLen + 1];
		memset(result, 0, resultLen + 1);
		for (int i = 0; i < function->module->moduleDecl->namespaces.size; i++)
		{
			strcpy(result + strlen(result), function->module->moduleDecl->namespaces[i]);
			strcpy(result + strlen(result), "__");
		}
		strcpy(result + strlen(result), function->name);
		return result;
	}
	else
	{
		return _strdup(function->name);
	}
}
