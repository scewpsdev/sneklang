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
	//if (AST::File* file = module->file)
	for (AST::File* file : module->files)
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

bool Resolver::findFunctionsInFile(const char* name, AST::File* file, List<AST::Function*>& functions)
{
	bool found = false;

	for (int i = 0; i < file->functions.size; i++)
	{
		AST::Function* function = file->functions[i];
		if (function->visibility == AST::Visibility::Public || function->file == currentFile)
		{
			if (strcmp(function->name, name) == 0)
			{
				functions.add(function);
				found = true;
			}
		}
	}

	return found;
}

bool Resolver::findFunctionsInModule(const char* name, AST::Module* module, List<AST::Function*>& functions)
{
	//if (AST::File* file = module->file)
	for (AST::File* file : module->files)
	{
		return findFunctionsInFile(name, file, functions);
	}
	return false;
}

bool Resolver::findFunctions(const char* name, List<AST::Function*>& functions)
{
	bool found = false;

	AST::Module* module = currentFile->moduleDecl ? currentFile->moduleDecl->module : globalNamespace;
	if (findFunctionsInModule(name, module, functions))
		return true;

	for (int i = 0; i < currentFile->dependencies.size; i++)
	{
		AST::Module* dependency = currentFile->dependencies[i];
		if (findFunctionsInModule(name, dependency, functions))
			found = true;
	}

	return found;
}

int Resolver::getFunctionOverloadScore(const AST::Function* function, const List<AST::Expression*>& arguments)
{
	if (arguments.size != function->paramTypes.size)
		return INT32_MAX;

	int score = 0;

	for (int i = 0; i < arguments.size; i++)
	{
		if (CompareTypes(arguments[i]->valueType, function->paramTypes[i]->typeID))
			;
		else if (function->isGenericParam(i))
			score++;
		else if (CanConvertImplicit(arguments[i]->valueType, function->paramTypes[i]->typeID, arguments[i]->isConstant()))
			score += 2;
		else
			score = INT32_MAX;
	}

	if (!isFunctionVisible(function, currentFile->module))
		score += arguments.size * 2 + 1;

	return score;
}

void Resolver::chooseFunctionOverload(List<AST::Function*>& functions, const List<AST::Expression*>& arguments)
{
	if (functions.size <= 1)
		return;

	static const List<AST::Expression*>* argumentsRef = nullptr;
	argumentsRef = &arguments;

	static Resolver* resolver = nullptr;
	resolver = this;

	functions.sort([](AST::Function* const* function1, AST::Function* const* function2) -> int
		{
			const List<AST::Expression*>& arguments = *argumentsRef;

			int score1 = resolver->getFunctionOverloadScore(*function1, arguments);
			int score2 = resolver->getFunctionOverloadScore(*function2, arguments);

			return score1 < score2 ? -1 : score1 > score2 ? 1 : 0;
		});

	for (int i = functions.size - 1; i >= 0; i--)
	{
		if (getFunctionOverloadScore(functions[i], arguments) == INT32_MAX)
		{
			functions.removeAt(i);
		}
	}
}
