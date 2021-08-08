#pragma once

#include "ast.h"


AstVariable* FindVariable(Resolver* resolver, const char* name);
AstFunction* FindFunction(Resolver* resolver, const char* name);
