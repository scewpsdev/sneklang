#pragma once

#include "File.h"
#include "utils/List.h"


namespace AST
{
	struct Module
	{
		char* name;
		Module* parent;
		List<Module*> children;
		List<File*> files;

		Module(const char* name, Module* parent);
		~Module();
	};
}
