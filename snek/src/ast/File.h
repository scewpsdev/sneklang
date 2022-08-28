#pragma once

#include "Type.h"
#include "Declaration.h"
#include "utils/List.h"


struct SourceFile;

namespace AST
{
	struct Module;
	struct File;
	struct Expression;


	struct File
	{
		char* name;
		int moduleID;
		SourceFile* sourceFile;

		Module* module = nullptr;
		char* nameSpace = nullptr;

		ModuleDeclaration* moduleDecl = nullptr;
		NamespaceDeclaration* namespaceDecl = nullptr;

		List<Function*> functions;
		List<Struct*> structs;
		List<Class*> classes;
		List<Typedef*> typedefs;
		List<Enum*> enums;
		List<Exprdef*> exprdefs;
		List<GlobalVariable*> globals;
		List<Import*> imports;

		List<Module*> dependencies;


		File(char* name, int moduleID, SourceFile* sourceFile);
	};
}
