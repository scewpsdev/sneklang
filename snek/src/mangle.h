#pragma once


#define ENTRY_POINT_NAME "main"


struct Resolver;

namespace AST
{
	struct Function;
}

char* MangleFunctionName(Resolver* resolver, AST::Function* function);
