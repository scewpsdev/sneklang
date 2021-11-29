#include "resolver.h"

#include "ast.h"
#include "log.h"


AstVariable* RegisterLocalVariable(Resolver* resolver, TypeID type, AstExpression* value, const char* name, bool isConstant, AstFile* module)
{
	Scope* scope = resolver->scope;

	AstVariable* variable = CreateAstVariable(resolver->file, resolver->currentElement->inputState);
	variable->name = _strdup(name);
	variable->mangledName = _strdup(name); // TODO Mangle
	variable->isConstant = isConstant;
	variable->type = type;
	variable->value = value;
	variable->module = module;

	scope->localVariables.add(variable);

	return variable;
}

AstVariable* RegisterGlobalVariable(Resolver* resolver, TypeID type, AstExpression* value, const char* name, bool isConstant, AstVisibility visibility, AstFile* module, AstGlobal* declaration)
{
	AstVariable* variable = CreateAstVariable(resolver->file, resolver->currentElement->inputState);
	variable->name = _strdup(name);
	variable->mangledName = _strdup(name); // TODO Mangle
	variable->isConstant = isConstant;
	variable->visibility = visibility;
	variable->type = type;
	variable->value = value;
	variable->module = module;
	variable->globalDecl = declaration;

	SnekAssert(variable);

	return variable;
}

static AstVariable* FindVariableInScope(Resolver* resolver, const char* name, Scope* scope, bool recursive)
{
	if (!scope)
		return NULL;
	for (int i = 0; i < scope->localVariables.size; i++)
	{
		AstVariable* variable = scope->localVariables[i];
		if (strcmp(variable->name, name) == 0)
		{
			return variable;
		}
	}
	if (recursive && scope->parent)
	{
		return FindVariableInScope(resolver, name, scope->parent, recursive);
	}

	return NULL;
}

AstVariable* FindGlobalVariableInFile(Resolver* resolver, AstFile* file, const char* name)
{
	for (int i = 0; i < file->globals.size; i++)
	{
		AstVariable* variable = file->globals[i]->variable;
		if (strcmp(variable->name, name) == 0)
		{
			return variable;
		}
	}
	return NULL;
}

AstVariable* FindGlobalVariableInNamespace(Resolver* resolver, AstModule* ns, const char* name, AstModule* currentNamespace)
{
	for (int i = 0; i < ns->files.size; i++)
	{
		AstFile* module = ns->files[i];
		if (AstVariable* variable = FindGlobalVariableInFile(resolver, module, name))
		{
			if (variable->visibility >= VISIBILITY_PUBLIC || currentNamespace == ns)
				return variable;
			else
			{
				SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Variable '%s' is not visible", variable->name);
				return NULL;
			}
		}
	}
	return NULL;
}

AstVariable* FindVariable(Resolver* resolver, const char* name)
{
	if (AstVariable* variable = FindVariableInScope(resolver, name, resolver->scope, true))
		return variable;

	if (AstVariable* variable = FindGlobalVariableInFile(resolver, resolver->file, name))
	{
		return variable;
	}

	AstModule* ns = resolver->file->moduleDecl ? resolver->file->moduleDecl->ns : resolver->globalNamespace;
	for (int i = 0; i < ns->files.size; i++)
	{
		AstFile* module = ns->files[i];
		if (module != resolver->file)
		{
			if (AstVariable* variable = FindGlobalVariableInFile(resolver, resolver->file, name))
			{
				if (variable->visibility >= VISIBILITY_PUBLIC)
					return variable;
				else
				{
					SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Variable '%s' is not visible", variable->name);
					return NULL;
				}
			}
		}
	}
	for (int i = 0; i < resolver->file->dependencies.size; i++)
	{
		AstModule* dependency = resolver->file->dependencies[i];
		for (int j = 0; j < dependency->files.size; j++)
		{
			AstFile* module = dependency->files[j];
			if (AstVariable* variable = FindGlobalVariableInFile(resolver, module, name))
			{
				if (variable->visibility >= VISIBILITY_PUBLIC)
					return variable;
				else
				{
					SnekError(resolver->context, resolver->currentElement->inputState, ERROR_CODE_NON_VISIBLE_DECLARATION, "Variable '%s' is not visible", variable->name);
					return NULL;
				}
			}
		}
	}
	return NULL;
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
