#pragma once

#include "List.h"
#include "Type.h"
#include "ast/Declaration.h"

#include <map>


struct SkContext;
struct Variable;

namespace AST
{
	struct Element;
	struct Module;
	struct File;
	struct Function;
	struct Statement;
	struct Expression;
}

struct Scope
{
	Scope* parent = NULL;
	const char* name;

	AST::Statement* branchDst;

	List<Variable*> localVariables;
};

struct Resolver
{
	SkContext* context;

	AST::Module* globalNamespace = nullptr;

	AST::File* currentFile = nullptr;
	AST::Element* currentElement = nullptr;
	AST::Function* currentFunction = nullptr;
	AST::Struct* currentStruct = nullptr;
	//AstStatement* currentStatement;
	//AstExpression* currentExpression;

	Scope* scope = nullptr;

	Resolver(SkContext* context);
	~Resolver();

	bool run();

	Scope* pushScope(const char* name);
	void popScope();

	AST::File* findFileByName(const char* name);

	void registerLocalVariable(Variable* variable, AST::Element* declaration);
	void registerGlobalVariable(Variable* variable, AST::GlobalVariable* global);

	TypeID getGenericTypeArgument(const char* name);

	Variable* findLocalVariableInScope(const char* name, Scope* scope, bool recursive);
	Variable* findGlobalVariableInFile(const char* name, AST::File* file);
	Variable* findGlobalVariableInModule(const char* name, AST::Module* module, AST::Module* current);
	Variable* findVariable(const char* name);

	AST::Function* findFunctionInFile(const char* name, AST::File* file);
	AST::Function* findFunctionInModule(const char* name, AST::Module* module);
	AST::Function* findFunction(const char* name);

	AST::Enum* findEnumInFile(const char* name, AST::File* file);
	AST::Enum* findEnumInModule(const char* name, AST::Module* module);
	AST::Enum* findEnum(const char* name);

	AST::EnumValue* findEnumValueInFile(const char* name, AST::File* file);
	AST::EnumValue* findEnumValueInModule(const char* name, AST::Module* module);
	AST::EnumValue* findEnumValue(const char* name);

	AST::Struct* findStructInFile(const char* name, AST::File* file);
	AST::Struct* findStructInModule(const char* name, AST::Module* module);
	AST::Struct* findStruct(const char* name);

	AST::Typedef* findTypedefInFile(const char* name, AST::File* file);
	AST::Typedef* findTypedefInModule(const char* name, AST::Module* module);
	AST::Typedef* findTypedef(const char* name);

	AST::Exprdef* findExprdefInFile(const char* name, AST::File* file);
	AST::Exprdef* findExprdefInModule(const char* name, AST::Module* module);
	AST::Exprdef* findExprdef(const char* name);
};
