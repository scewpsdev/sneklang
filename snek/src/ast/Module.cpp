#include "Module.h"


namespace AST
{
	Module::Module(const char* name, Module* parent)
		: name(_strdup(name)), parent(parent)
	{
	}

	Module::~Module()
	{
		delete name;
		for (Module* child : children)
			delete child;
		DestroyList(children);
	}
}
