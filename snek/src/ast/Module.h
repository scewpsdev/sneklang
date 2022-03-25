#pragma once

#include "File.h"
#include "List.h"


namespace AST
{
	struct Module
	{
		char* name;
		Module* parent;
		List<Module*> children;
		File* file = nullptr;

		Module(const char* name, Module* parent);
		~Module();
	};
}
