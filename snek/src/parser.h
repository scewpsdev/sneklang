#pragma once

#include "list.h"
#include "lexer.h"
#include "log.h"


struct SkContext;
struct AstModule;

struct Parser
{
	SkContext* context;
	Lexer* lexer;

	AstModule* module;
};


Parser* CreateParser(SkContext* context);
void DestroyParser(Parser* parser);

void ParserRun(Parser* parser);
