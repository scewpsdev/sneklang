#pragma once


#define ENTRY_POINT_NAME "main"


struct Resolver;

namespace AST
{
	struct Function;
	struct Struct;
}

char* MangleFunctionName(AST::Function* function);
char* MangleStructName(AST::Struct* str);
