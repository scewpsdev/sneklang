#pragma once

#include "Lexer.h"
#include "utils/List.h"
#include "utils/Log.h"


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
