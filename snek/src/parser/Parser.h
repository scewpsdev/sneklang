#pragma once

#include "List.h"
#include "lexer.h"
#include "log.h"


struct SkContext;

namespace AST
{
	struct File;
}

struct Parser
{
	SkContext* context;
	Lexer* lexer;

	// TODO rename to "file"
	AST::File* module;

	bool failed;
};


Parser* CreateParser(SkContext* context);
void DestroyParser(Parser* parser);

bool ParserRun(Parser* parser);
