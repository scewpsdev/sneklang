#pragma once

#include "List.h"
#include "lexer.h"
#include "log.h"


struct SkContext;
struct AstFile;

struct Parser
{
	SkContext* context;
	Lexer* lexer;

	AstFile* module;

	bool failed;
};


Parser* CreateParser(SkContext* context);
void DestroyParser(Parser* parser);

bool ParserRun(Parser* parser);
