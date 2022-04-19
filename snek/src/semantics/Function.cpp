#include "Function.h"

#include "Resolver.h"
#include "ast/File.h"
#include "ast/Declaration.h"
#include "ast/Module.h"


AST::Function* Resolver::findFunctionInFile(const char* name, AST::File* file)
{
	for (int i = 0; i < file->functions.size; i++)
	{
		AST::Function* function = file->functions[i];
		if (function->visibility == AST::Visibility::Public || function->file == currentFile)
		{
			if (strcmp(function->name, name) == 0)
			{
				return function;
			}
		}
	}
	return nullptr;
}

AST::Function* Resolver::findFunctionInModule(const char* name, AST::Module* module)
{
	if (AST::File* file = module->file)
	{
		return findFunctionInFile(name, file);
	}
	return nullptr;
}

AST::Function* Resolver::findFunction(const char* name)
{
	if (AST::Function* function = findFunctionInFile(name, currentFile))
	{
		return function;
	}

	AST::Module* module = currentFile->moduleDecl ? currentFile->moduleDecl->module : globalNamespace;
	if (AST::Function* function = findFunctionInModule(name, module))
	{
		return function;
	}
	for (int i = 0; i < currentFile->dependencies.size; i++)
	{
		AST::Module* dependency = currentFile->dependencies[i];
		if (AST::Function* function = findFunctionInModule(name, dependency))
		{
			return function;
		}
	}
	return nullptr;
}
