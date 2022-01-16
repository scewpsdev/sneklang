#pragma once

#include "ast/Element.h"
#include "ast/Module.h"
#include "ast/File.h"
#include "ast/Declaration.h"

#include "semantics/Type.h"


struct Variable
{
	AST::File* file;
	char* name;
	bool isConstant;
	AST::Visibility visibility;
	AST::Expression* value;

	AST::GlobalVariable* globalDecl = nullptr;

	char* mangledName = nullptr;
	TypeID type = nullptr;

	ValueHandle allocHandle = nullptr;


	Variable(AST::File* file, char* name, TypeID type, AST::Expression* value, bool isConstant, AST::Visibility visibility);
};


void RegisterLocalVariable(Resolver* resolver, TypeID type, AST::Expression* value, const char* name, bool isConstant, AST::File* file, const AST::SourceLocation& location);
void RegisterGlobalVariable(Resolver* resolver, TypeID type, AST::Expression* value, const char* name, bool isConstant, AST::Visibility visibility, AST::File* module, AST::GlobalVariable* declaration);

Variable* FindGlobalVariableInFile(Resolver* resolver, AST::File* file, const char* name);
Variable* FindGlobalVariableInNamespace(Resolver* resolver, AST::Module* module, const char* name, AST::Module* currentModule);
Variable* FindVariable(Resolver* resolver, const char* name);

AST::Function* FindFunctionInFile(Resolver* resolver, AST::File* file, const char* name);
AST::Function* FindFunctionInNamespace(Resolver* resolver, AST::Module* module, const char* name, AST::Module* currentModule);
AST::Function* FindFunction(Resolver* resolver, const char* name);

AST::EnumValue* FindEnumValue(Resolver* resolver, const char* name);

AST::Struct* FindStruct(Resolver* resolver, const char* name);

AST::Class* FindClass(Resolver* resolver, const char* name);

AST::Typedef* FindTypedef(Resolver* resolver, const char* name);

AST::Enum* FindEnum(Resolver* resolver, const char* name);

AST::Exprdef* FindExprdef(Resolver* resolver, const char* name);
