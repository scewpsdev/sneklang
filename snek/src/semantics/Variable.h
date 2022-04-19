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

	AST::Element* declaration = nullptr;

	char* mangledName = nullptr;
	TypeID type = nullptr;

	ValueHandle allocHandle = nullptr;


	Variable(AST::File* file, char* name, TypeID type, AST::Expression* value, bool isConstant, AST::Visibility visibility);
	~Variable();
};


AST::EnumValue* FindEnumValue(Resolver* resolver, const char* name);

AST::Struct* FindStruct(Resolver* resolver, const char* name);

AST::Class* FindClass(Resolver* resolver, const char* name);

AST::Typedef* FindTypedef(Resolver* resolver, const char* name);

AST::Enum* FindEnum(Resolver* resolver, const char* name);

AST::Exprdef* FindExprdef(Resolver* resolver, const char* name);
