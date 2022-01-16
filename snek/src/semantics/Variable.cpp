#include "Variable.h"

#include "Resolver.h"

#include "log.h"


Variable::Variable(AST::File* file, char* name, TypeID type, AST::Expression* value, bool isConstant, AST::Visibility visibility)
	: file(file), name(name), type(type), value(value), isConstant(isConstant), visibility(visibility)
{
	mangledName = _strdup(name); // TODO Mangle
}

void Resolver::registerLocalVariable(Variable* variable)
{
	scope->localVariables.add(variable);
}

void Resolver::registerGlobalVariable(Variable* variable, AST::GlobalVariable* global)
{
	variable->globalDecl = global;
}

Variable* Resolver::findLocalVariableInScope(const char* name, Scope* scope, bool recursive)
{
	if (!scope)
		return nullptr;
	for (int i = 0; i < scope->localVariables.size; i++)
	{
		Variable* variable = scope->localVariables[i];
		if (strcmp(variable->name, name) == 0)
		{
			return variable;
		}
	}
	if (recursive && scope->parent)
	{
		return findLocalVariableInScope(name, scope->parent, recursive);
	}

	return nullptr;
}

Variable* Resolver::findGlobalVariableInFile(const char* name, AST::File* file)
{
	for (int i = 0; i < file->globals.size; i++)
	{
		for (int j = 0; j < file->globals[i]->declarators.size; j++)
		{
			Variable* variable = file->globals[i]->declarators[j]->variable;
			if (strcmp(variable->name, name) == 0)
			{
				return variable;
			}
		}
	}
	return nullptr;
}

Variable* Resolver::findGlobalVariableInModule(const char* name, AST::Module* module, AST::Module* current)
{
	for (int i = 0; i < module->files.size; i++)
	{
		AST::File* file = module->files[i];
		if (Variable* variable = findGlobalVariableInFile(name, file))
		{
			if (variable->visibility >= AST::Visibility::Public || current == module)
				return variable;
			else
			{
				SnekError(context, currentElement->location, ERROR_CODE_NON_VISIBLE_DECLARATION, "Variable '%s' is not visible", variable->name);
				return nullptr;
			}
		}
	}
	return nullptr;
}

Variable* Resolver::findVariable(const char* name)
{
	if (Variable* variable = findLocalVariableInScope(name, scope, true))
		return variable;

	if (Variable* variable = findGlobalVariableInFile(name, currentFile))
		return variable;

	AST::Module* module = currentFile->moduleDecl ? currentFile->moduleDecl->ns : globalNamespace;
	for (int i = 0; i < module->files.size; i++)
	{
		AST::File* file = module->files[i];
		if (file != currentFile)
		{
			if (Variable* variable = findGlobalVariableInFile(name, currentFile))
			{
				if (variable->visibility >= AST::Visibility::Public)
					return variable;
				else
				{
					SnekError(context, currentElement->location, ERROR_CODE_NON_VISIBLE_DECLARATION, "Variable '%s' is not visible", variable->name);
					return nullptr;
				}
			}
		}
	}
	for (int i = 0; i < currentFile->dependencies.size; i++)
	{
		AST::Module* dependency = currentFile->dependencies[i];
		for (int j = 0; j < dependency->files.size; j++)
		{
			AST::File* file = dependency->files[j];
			if (Variable* variable = findGlobalVariableInFile(name, file))
			{
				if (variable->visibility >= AST::Visibility::Public)
					return variable;
				else
				{
					SnekError(context, currentElement->location, ERROR_CODE_NON_VISIBLE_DECLARATION, "Variable '%s' is not visible", variable->name);
					return nullptr;
				}
			}
		}
	}
	return nullptr;
}

AstFunction* FindFunctionInFile(Resolver* resolver, AstFile* file, const char* name)
{
	for (int i = 0; i < file->functions.size; i++)
	{
		AstFunction* function = file->functions[i];
		if (strcmp(function->name, name) == 0)
		{
			return function;
		}
	}
	return NULL;
}

AstFunction* FindFunctionInNamespace(Resolver* resolver, AstModule* ns, const char* name, AstModule* currentNamespace)
{
	for (int i = 0; i < ns->files.size; i++)
	{
		AstFile* module = ns->files[i];
		if (AstFunction* function = FindFunctionInFile(resolver, module, name))
		{
			if (function->visibility >= VISIBILITY_PUBLIC || currentNamespace == ns)
				return function;
			else
			{
				SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Function '%s' is not visible", function->name);
				return NULL;
			}
		}
	}
	return NULL;
}

AstFunction* FindFunction(Resolver* resolver, const char* name)
{
	if (AstFunction* function = FindFunctionInFile(resolver, resolver->file, name))
	{
		return function;
	}

	AstModule* ns = resolver->file->moduleDecl ? resolver->file->moduleDecl->ns : resolver->globalNamespace;
	if (AstFunction* function = FindFunctionInNamespace(resolver, ns, name, ns))
	{
		return function;
	}
	for (int i = 0; i < resolver->file->dependencies.size; i++)
	{
		AstModule* dependency = resolver->file->dependencies[i];
		if (AstFunction* function = FindFunctionInNamespace(resolver, dependency, name, ns))
		{
			return function;
		}
	}
	return NULL;
}

AstEnumValue* FindEnumValueInFile(Resolver* resolver, AstFile* file, const char* name)
{
	for (int i = 0; i < file->enums.size; i++)
	{
		AstEnum* enumDecl = file->enums[i];
		for (int j = 0; j < enumDecl->values.size; j++)
		{
			if (strcmp(enumDecl->values[j].name, name) == 0)
			{
				return &enumDecl->values[j];
			}
		}
	}
	return NULL;
}

AstEnumValue* FindEnumValueInNamespace(Resolver* resolver, AstModule* ns, const char* name, AstModule* currentNamespace)
{
	for (int i = 0; i < ns->files.size; i++)
	{
		AstFile* module = ns->files[i];
		if (AstEnumValue* enumValue = FindEnumValueInFile(resolver, module, name))
		{
			if (enumValue->enumDecl->visibility >= VISIBILITY_PUBLIC || currentNamespace == ns)
				return enumValue;
			else
			{
				SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Enum '%s' containing value '%s' is not visible", enumValue->enumDecl->name, enumValue->name);
				return NULL;
			}
		}
	}
	return NULL;
}

AstEnumValue* FindEnumValue(Resolver* resolver, const char* name)
{
	if (AstEnumValue* enumValue = FindEnumValueInFile(resolver, resolver->file, name))
	{
		return enumValue;
	}

	AstModule* ns = resolver->file->moduleDecl ? resolver->file->moduleDecl->ns : resolver->globalNamespace;
	if (AstEnumValue* enumValue = FindEnumValueInNamespace(resolver, ns, name, ns))
	{
		return enumValue;
	}
	for (int i = 0; i < resolver->file->dependencies.size; i++)
	{
		AstModule* dependency = resolver->file->dependencies[i];
		if (AstEnumValue* enumValue = FindEnumValueInNamespace(resolver, dependency, name, ns))
		{
			return enumValue;
		}
	}
	return NULL;
}

static AstStruct* FindStructInModule(Resolver* resolver, AstFile* module, const char* name)
{
	for (int i = 0; i < module->structs.size; i++)
	{
		AstStruct* str = module->structs[i];
		if (strcmp(str->name, name) == 0)
		{
			return str;
		}
	}
	return NULL;
}

AstStruct* FindStruct(Resolver* resolver, const char* name)
{
	if (AstStruct* str = FindStructInModule(resolver, resolver->file, name))
	{
		return str;
	}

	AstModule* ns = resolver->file->moduleDecl ? resolver->file->moduleDecl->ns : resolver->globalNamespace;
	for (int i = 0; i < ns->files.size; i++)
	{
		AstFile* module = ns->files[i];
		if (AstStruct* str = FindStructInModule(resolver, module, name))
		{
			if (str->visibility >= VISIBILITY_PUBLIC)
				return str;
			else
			{
				SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Struct '%s' is not visible", str->name);
				return NULL;
			}
		}
	}
	for (int i = 0; i < resolver->file->dependencies.size; i++)
	{
		AstModule* dependency = resolver->file->dependencies[i];
		for (int j = 0; j < dependency->files.size; j++)
		{
			AstFile* module = dependency->files[j];
			if (AstStruct* str = FindStructInModule(resolver, module, name))
			{
				if (str->visibility >= VISIBILITY_PUBLIC)
					return str;
				else
				{
					SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Struct '%s' is not visible", str->name);
					return NULL;
				}
			}
		}
	}
	return NULL;
}

static AstClass* FindClassInModule(Resolver* resolver, AstFile* module, const char* name)
{
	for (int i = 0; i < module->classes.size; i++)
	{
		AstClass* str = module->classes[i];
		if (strcmp(str->name, name) == 0)
		{
			return str;
		}
	}
	return NULL;
}

AstClass* FindClass(Resolver* resolver, const char* name)
{
	if (AstClass* clss = FindClassInModule(resolver, resolver->file, name))
	{
		return clss;
	}

	AstModule* ns = resolver->file->moduleDecl ? resolver->file->moduleDecl->ns : resolver->globalNamespace;
	for (int i = 0; i < ns->files.size; i++)
	{
		AstFile* module = ns->files[i];
		if (AstClass* clss = FindClassInModule(resolver, module, name))
		{
			if (clss->visibility >= VISIBILITY_PUBLIC)
				return clss;
			else
			{
				SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Class '%s' is not visible", clss->name);
				return NULL;
			}
		}
	}
	for (int i = 0; i < resolver->file->dependencies.size; i++)
	{
		AstModule* dependency = resolver->file->dependencies[i];
		for (int j = 0; j < dependency->files.size; j++)
		{
			AstFile* module = dependency->files[j];
			if (AstClass* clss = FindClassInModule(resolver, module, name))
			{
				if (clss->visibility >= VISIBILITY_PUBLIC)
					return clss;
				else
				{
					SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Class '%s' is not visible", clss->name);
					return NULL;
				}
			}
		}
	}
	return NULL;
}

static AstTypedef* FindTypedefInModule(Resolver* resolver, AstFile* module, const char* name)
{
	for (int i = 0; i < module->typedefs.size; i++)
	{
		AstTypedef* td = module->typedefs[i];
		if (strcmp(td->name, name) == 0)
		{
			return td;
		}
	}
	return NULL;
}

AstTypedef* FindTypedef(Resolver* resolver, const char* name)
{
	if (AstTypedef* td = FindTypedefInModule(resolver, resolver->file, name))
	{
		return td;
	}

	AstModule* ns = resolver->file->moduleDecl ? resolver->file->moduleDecl->ns : resolver->globalNamespace;
	for (int i = 0; i < ns->files.size; i++)
	{
		AstFile* module = ns->files[i];
		if (AstTypedef* td = FindTypedefInModule(resolver, module, name))
		{
			if (td->visibility >= VISIBILITY_PUBLIC)
				return td;
			else
			{
				SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Typedef '%s' is not visible", td->name);
				return NULL;
			}
		}
	}
	for (int i = 0; i < resolver->file->dependencies.size; i++)
	{
		AstModule* dependency = resolver->file->dependencies[i];
		for (int j = 0; j < dependency->files.size; j++)
		{
			AstFile* module = dependency->files[j];
			if (AstTypedef* td = FindTypedefInModule(resolver, module, name))
			{
				if (td->visibility >= VISIBILITY_PUBLIC)
					return td;
				else
				{
					SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Typedef '%s' is not visible", td->name);
					return NULL;
				}
			}
		}
	}
	return NULL;
}

static AstEnum* FindEnumInModule(Resolver* resolver, AstFile* module, const char* name)
{
	for (int i = 0; i < module->enums.size; i++)
	{
		AstEnum* en = module->enums[i];
		if (strcmp(en->name, name) == 0)
		{
			return en;
		}
	}
	return NULL;
}

AstEnum* FindEnum(Resolver* resolver, const char* name)
{
	if (AstEnum* en = FindEnumInModule(resolver, resolver->file, name))
	{
		return en;
	}

	AstModule* ns = resolver->file->moduleDecl ? resolver->file->moduleDecl->ns : resolver->globalNamespace;
	for (int i = 0; i < ns->files.size; i++)
	{
		AstFile* module = ns->files[i];
		if (AstEnum* en = FindEnumInModule(resolver, module, name))
		{
			if (en->visibility >= VISIBILITY_PUBLIC)
				return en;
			else
			{
				SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Enum '%s' is not visible", en->name);
				return NULL;
			}
		}
	}
	for (int i = 0; i < resolver->file->dependencies.size; i++)
	{
		AstModule* dependency = resolver->file->dependencies[i];
		for (int j = 0; j < dependency->files.size; j++)
		{
			AstFile* module = dependency->files[j];
			if (AstEnum* en = FindEnumInModule(resolver, module, name))
			{
				if (en->visibility >= VISIBILITY_PUBLIC)
					return en;
				else
				{
					SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Enum '%s' is not visible", en->name);
					return NULL;
				}
			}
		}
	}
	return NULL;
}

static AstExprdef* FindExprdefInModule(Resolver* resolver, AstFile* module, const char* name)
{
	for (int i = 0; i < module->exprdefs.size; i++)
	{
		AstExprdef* ed = module->exprdefs[i];
		if (strcmp(ed->name, name) == 0)
		{
			return ed;
		}
	}
	return NULL;
}

AstExprdef* FindExprdef(Resolver* resolver, const char* name)
{
	if (AstExprdef* ed = FindExprdefInModule(resolver, resolver->file, name))
	{
		return ed;
	}

	AstModule* ns = resolver->file->moduleDecl ? resolver->file->moduleDecl->ns : resolver->globalNamespace;
	for (int i = 0; i < ns->files.size; i++)
	{
		AstFile* module = ns->files[i];
		if (AstExprdef* ed = FindExprdefInModule(resolver, module, name))
		{
			if (ed->visibility >= VISIBILITY_PUBLIC)
				return ed;
			else
			{
				SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Exprdef '%s' is not visible", ed->name);
				return NULL;
			}
		}
	}
	for (int i = 0; i < resolver->file->dependencies.size; i++)
	{
		AstModule* dependency = resolver->file->dependencies[i];
		for (int j = 0; j < dependency->files.size; j++)
		{
			AstFile* module = dependency->files[j];
			if (AstExprdef* ed = FindExprdefInModule(resolver, module, name))
			{
				if (ed->visibility >= VISIBILITY_PUBLIC)
					return ed;
				else
				{
					SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Exprdef '%s' is not visible", ed->name);
					return NULL;
				}
			}
		}
	}
	return NULL;
}
