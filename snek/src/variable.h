#pragma once

#include "ast.h"


struct Resolver;


AstVariable* RegisterLocalVariable(Resolver* resolver, TypeID type, AstExpression* value, const char* name, bool isConstant, AstFile* module);
AstVariable* RegisterGlobalVariable(Resolver* resolver, TypeID type, AstExpression* value, const char* name, bool isConstant, AstVisibility visibility, AstFile* module, AstGlobal* declaration);

AstVariable* FindGlobalVariableInFile(Resolver* resolver, AstFile* file, const char* name);
AstVariable* FindGlobalVariableInNamespace(Resolver* resolver, AstModule* module, const char* name, AstModule* currentModule);
AstVariable* FindVariable(Resolver* resolver, const char* name);

AstFunction* FindFunctionInFile(Resolver* resolver, AstFile* file, const char* name);
AstFunction* FindFunctionInNamespace(Resolver* resolver, AstModule* module, const char* name, AstModule* currentModule);
AstFunction* FindFunction(Resolver* resolver, const char* name);

AstEnumValue* FindEnumValue(Resolver* resolver, const char* name);

AstStruct* FindStruct(Resolver* resolver, const char* name);

AstClass* FindClass(Resolver* resolver, const char* name);

AstTypedef* FindTypedef(Resolver* resolver, const char* name);

AstEnum* FindEnum(Resolver* resolver, const char* name);

AstExprdef* FindExprdef(Resolver* resolver, const char* name);
