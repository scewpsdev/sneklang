#include "parser.h"

#include "snek.h"
#include "ast.h"
#include "lexer.h"
#include "log.h"

#include <string.h>
#include <stdlib.h>


Parser* CreateParser(SkContext* context)
{
	Parser* parser = new Parser();

	parser->context = context;

	return parser;
}

void DestroyParser(Parser* parser)
{
	delete parser;
}

static InputState GetInputState(Parser* parser)
{
	return parser->lexer->input.state;
}

static void SetInputState(Parser* parser, const InputState& inputState)
{
	parser->lexer->input.state = inputState;
}

static bool HasNext(Parser* parser)
{
	return LexerHasNext(parser->lexer);
}

static Token NextToken(Parser* parser)
{
	return LexerNext(parser->lexer);
}

static Token PeekToken(Parser* parser)
{
	return LexerPeek(parser->lexer);
}

static bool NextTokenIs(Parser* parser, int tokenType)
{
	return PeekToken(parser).type == tokenType;
}

static bool NextTokenIsKeyword(Parser* parser, int keywordType)
{
	Token tok = PeekToken(parser);
	return tok.type == TOKEN_TYPE_IDENTIFIER && tok.keywordType == keywordType;
}

static void SkipToken(Parser* parser, int tokenType)
{
	if (NextTokenIs(parser, tokenType))
		NextToken(parser);
	else
	{
		// TODO ERROR
		SnekAssert(false, "");
	}
}

static AstType* ParseElementType(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	Token tok = NextToken(parser);
	if (tok.type == TOKEN_TYPE_IDENTIFIER)
	{
		switch (tok.keywordType)
		{
		case KEYWORD_TYPE_VOID:
			return CreateAstType<AstVoidType>(parser->module, inputState, TYPE_KIND_VOID);

		case KEYWORD_TYPE_INT8:
		{
			auto type = CreateAstType<AstIntegerType>(parser->module, inputState, TYPE_KIND_INTEGER);
			type->bitWidth = 8;
			type->isSigned = true;
			return type;
		}
		case KEYWORD_TYPE_INT16:
		{
			auto type = CreateAstType<AstIntegerType>(parser->module, inputState, TYPE_KIND_INTEGER);
			type->bitWidth = 16;
			type->isSigned = true;
			return type;
		}
		case KEYWORD_TYPE_INT32:
		{
			auto type = CreateAstType<AstIntegerType>(parser->module, inputState, TYPE_KIND_INTEGER);
			type->bitWidth = 32;
			type->isSigned = true;
			return type;
		}
		case KEYWORD_TYPE_INT64:
		{
			auto type = CreateAstType<AstIntegerType>(parser->module, inputState, TYPE_KIND_INTEGER);
			type->bitWidth = 64;
			type->isSigned = true;
			return type;
		}

		case KEYWORD_TYPE_UINT8:
		{
			auto type = CreateAstType<AstIntegerType>(parser->module, inputState, TYPE_KIND_INTEGER);
			type->bitWidth = 8;
			type->isSigned = false;
			return type;
		}
		case KEYWORD_TYPE_UINT16:
		{
			auto type = CreateAstType<AstIntegerType>(parser->module, inputState, TYPE_KIND_INTEGER);
			type->bitWidth = 16;
			type->isSigned = false;
			return type;
		}
		case KEYWORD_TYPE_UINT32:
		{
			auto type = CreateAstType<AstIntegerType>(parser->module, inputState, TYPE_KIND_INTEGER);
			type->bitWidth = 32;
			type->isSigned = false;
			return type;
		}
		case KEYWORD_TYPE_UINT64:
		{
			auto type = CreateAstType<AstIntegerType>(parser->module, inputState, TYPE_KIND_INTEGER);
			type->bitWidth = 64;
			type->isSigned = false;
			return type;
		}

		case KEYWORD_TYPE_FLOAT32:
		{
			auto type = CreateAstType<AstFPType>(parser->module, inputState, TYPE_KIND_FP);
			type->bitWidth = 32;
			return type;
		}
		case KEYWORD_TYPE_FLOAT64:
		{
			auto type = CreateAstType<AstFPType>(parser->module, inputState, TYPE_KIND_FP);
			type->bitWidth = 64;
			return type;
		}

		case KEYWORD_TYPE_BOOL:
			return CreateAstType<AstBoolType>(parser->module, inputState, TYPE_KIND_BOOL);
		}
	}

	SetInputState(parser, inputState);
	return NULL;
}

static AstType* ParseType(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	if (AstType* elementType = ParseElementType(parser))
	{
		if (NextTokenIs(parser, TOKEN_TYPE_OP_ASTERISK))
		{
			NextToken(parser); // *

			auto pointerType = CreateAstType<AstPointerType>(parser->module, inputState, TYPE_KIND_POINTER);
			pointerType->elementType = elementType;
			return pointerType;
		}
		return elementType;
	}

	return NULL;
}

static AstExpression* ParseExpression(Parser* parser);

static AstExpression* ParseAtom(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	if (NextTokenIs(parser, TOKEN_TYPE_INT_LITERAL))
	{
		char* str = GetTokenString(NextToken(parser));
		unsigned long long value = atoll(str);
		delete str;

		auto expr = CreateAstExpression<AstIntegerLiteral>(parser->module, inputState, EXPR_KIND_INTEGER_LITERAL);
		expr->value = value;

		return expr;
	}
	else if (NextTokenIs(parser, TOKEN_TYPE_FLOAT_LITERAL))
	{
		char* str = GetTokenString(NextToken(parser));
		double value = atof(str);
		delete str;

		auto expr = CreateAstExpression<AstFPLiteral>(parser->module, inputState, EXPR_KIND_FP_LITERAL);
		expr->value = value;

		return expr;
	}
	else if (NextTokenIs(parser, TOKEN_TYPE_CHAR_LITERAL))
	{
		Token tok = NextToken(parser);

		char value = 0;
		for (int i = 0; i < tok.len; i++) {
			char c = tok.str[i];
			if (c == '\\' && i < tok.len - 1) {
				i++;
				switch (tok.str[i]) {
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 't': c = '\t'; break;
				case 'b': c = '\b'; break;
				case '0': c = '\0'; break;
				case '\'': c = '\''; break;
				case '"': c = '"'; break;
				default:
					//PARSER_ERROR(parser, "Unknown escape character '\\%c'", token.str[i]);
					// TODO ERROR
					SnekAssert(false, "");
					break;
				}
			}
			value = c;

			if (i < tok.len - 1) {
				//PARSER_ERROR(parser, "Character literal must be of length 1");
				// TODO ERROR
				SnekAssert(false, "");
			}
			break;
		}

		auto expr = CreateAstExpression<AstCharacterLiteral>(parser->module, inputState, EXPR_KIND_CHARACTER_LITERAL);
		expr->value = value;

		return expr;
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_TRUE))
	{
		NextToken(parser); // true

		auto expr = CreateAstExpression<AstBoolLiteral>(parser->module, inputState, EXPR_KIND_BOOL_LITERAL);
		expr->value = true;
		return expr;
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_FALSE))
	{
		NextToken(parser); // false

		auto expr = CreateAstExpression<AstBoolLiteral>(parser->module, inputState, EXPR_KIND_BOOL_LITERAL);
		expr->value = false;
		return expr;
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_NULL_KEYWORD))
	{
		NextToken(parser); // null

		return CreateAstExpression<AstNullLiteral>(parser->module, inputState, EXPR_KIND_NULL_LITERAL);
	}
	else if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
	{
		char* name = GetTokenString(NextToken(parser));

		auto expr = CreateAstExpression<AstIdentifier>(parser->module, inputState, EXPR_KIND_IDENTIFIER);
		expr->name = name;
		return expr;
	}
	else if (NextTokenIs(parser, '('))
	{
		NextToken(parser); // (

		if (AstExpression* compoundValue = ParseExpression(parser)) // Compound
		{
			if (NextTokenIs(parser, ')'))
			{
				NextToken(parser); // )

				auto expr = CreateAstExpression<AstCompoundExpression>(parser->module, inputState, EXPR_KIND_COMPOUND);
				expr->value = compoundValue;
				return expr;
			}
		}

		// TODO ERROR
		SnekAssert(false, "");
		return NULL;
	}

	return NULL;
}

static AstExpression* ParseArgumentOperator(Parser* parser, AstExpression* expression)
{
	InputState inputState = GetInputState(parser);

	if (NextTokenIs(parser, '(')) // Function call
	{
		NextToken(parser); // (

		List<AstExpression*> arguments;

		bool upcomingDeclarator = !NextTokenIs(parser, ')');
		while (HasNext(parser) && upcomingDeclarator)
		{
			if (AstExpression* argument = ParseExpression(parser))
			{
				ListAdd(arguments, argument);

				upcomingDeclarator = NextTokenIs(parser, ',');
			}
			else
			{
				// TODO ERROR
				SnekAssert(false, "");
			}
		}

		SkipToken(parser, ')');

		auto expr = CreateAstExpression<AstFuncCall>(parser->module, inputState, EXPR_KIND_FUNC_CALL);
		expr->calleeExpr = expression;
		expr->arguments = arguments;
		return ParsePostfixOperator(parser, expr);
	}
	else if (NextTokenIs(parser, '[')) // Subscript operator
	{
		NextToken(parser); // [

		List<AstExpression*> arguments;

		bool upcomingDeclarator = !NextTokenIs(parser, ']');
		while (HasNext(parser) && upcomingDeclarator)
		{
			if (AstExpression* argument = ParseExpression(parser))
			{
				ListAdd(arguments, argument);

				upcomingDeclarator = NextTokenIs(parser, ',');
			}
			else
			{
				// TODO ERROR
				SnekAssert(false, "");
			}
		}

		SkipToken(parser, ']');

		auto expr = CreateAstExpression<AstSubscriptOperator>(parser->module, inputState, EXPR_KIND_SUBSCRIPT_OPERATOR);
		expr->operand = expression;
		expr->arguments = arguments;
		return ParsePostfixOperator(parser, expr);
	}
	else if (NextTokenIs(parser, '.')) // Dot operator
	{
		NextToken(parser); // .

		char* name = GetTokenString(NextToken(parser));

		auto expr = CreateAstExpression<AstDotOperator>(parser->module, inputState, EXPR_KIND_DOT_OPERATOR);
		expr->operand = expression;
		expr->name = name;
		return expr;
	}

	return expression;
}

static AstExpression* ParsePrefixOperator(Parser* parser)
{
	if (NextTokenIs(parser, TOKEN_TYPE_OP_EXCLAMATION))
	{
		NextToken(parser); // !
		if (AstExpression* operand= ParseAtom(parser))
		{
			operand = ParseArgumentOperator(parser, operand);

			// TODO .
		}
		else
		{
			SnekAssert(false, "");
			return NULL;
		}
	}
	else
	{
		if (AstExpression* atom = ParseAtom(parser))
		{
			return ParseArgumentOperator(parser, atom);
		}
		else
		{
			SnekAssert(false, "");
			return NULL;
		}
	}
}

static AstExpression* ParsePostfixOperator(Parser* parser, AstExpression* expression)
{
	return expression;
}

static AstExpression* ParseBasicExpression(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	if (NextTokenIs(parser, '('))
	{
		if (AstType* castType = ParseType(parser)) // Cast
		{
			if (NextTokenIs(parser, ')'))
			{
				NextToken(parser); // )
				if (AstExpression* value = ParsePrefixOperator(parser))
				{
					value = ParsePostfixOperator(parser, value);

					auto expr = CreateAstExpression<AstCast>(parser->module, inputState, EXPR_KIND_CAST);
					expr->value = value;
					expr->castType = castType;
					return expr;
				}
			}
		}
	}

	SetInputState(parser, inputState);
	if (AstExpression* expr = ParsePrefixOperator(parser))
	{
		expr = ParsePostfixOperator(parser, expr);
		return expr;
	}

	return NULL;
}

static AstExpression* ParseBinaryTernaryOperator(Parser* parser, AstExpression* expression)
{
	return expression;
}

static AstExpression* ParseExpression(Parser* parser)
{
	if (AstExpression* expr = ParseBasicExpression(parser))
	{
		expr = ParseBinaryTernaryOperator(parser, expr);
		return expr;
	}

	// TODO
	SnekAssert(false, "");
	return NULL;
}

static AstStatement* ParseStatement(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	if (NextTokenIs(parser, '{'))
	{
		NextToken(parser); // {

		List<AstStatement*> statements = CreateList<AstStatement*>();

		while (HasNext(parser) && !NextTokenIs(parser, '}'))
		{
			AstStatement* statement = ParseStatement(parser);
			ListAdd(statements, statement);
		}

		NextToken(parser); // }

		AstCompoundStatement* statement = CreateAstStatement<AstCompoundStatement>(parser->module, inputState, STATEMENT_KIND_COMPOUND);
		statement->statements = statements;
		return statement;
	}
	else if (AstType* type = ParseType(parser))
	{
		List<AstVarDeclarator> declarators;

		bool upcomingDeclarator = true;
		while (HasNext(parser) && upcomingDeclarator)
		{
			char* name = GetTokenString(NextToken(parser));
			AstExpression* value = NULL;

			if (NextTokenIs(parser, TOKEN_TYPE_OP_EQUALS))
			{
				NextToken(parser); // =
				value = ParseExpression(parser);
			}

			upcomingDeclarator = NextTokenIs(parser, ',');

			AstVarDeclarator declarator;
			declarator.name = name;
			declarator.value = value;

			ListAdd(declarators, declarator);
		}

		SkipToken(parser, ';');

		AstVarDeclStatement* statement = CreateAstStatement<AstVarDeclStatement>(parser->module, inputState, STATEMENT_KIND_VAR_DECL);
		statement->type = type;
		statement->declarators = declarators;

		return statement;
	}

	// TODO
	SnekAssert(false, "");
	return NULL;
}

static AstDeclaration* ParseDeclaration(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	if (AstType* type = ParseType(parser))
	{
		if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
		{
			char* name = GetTokenString(NextToken(parser));
			if (NextTokenIs(parser, '(')) // Function declaration
			{
				NextToken(parser); // (

				List<AstType*> paramTypes;
				List<char*> paramNames;

				bool upcomingDeclarator = !NextTokenIs(parser, ')');
				while (HasNext(parser) && upcomingDeclarator)
				{
					if (AstType* paramType = ParseType(parser))
					{
						if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
						{
							char* paramName = GetTokenString(NextToken(parser));

							ListAdd(paramTypes, paramType);
							ListAdd(paramNames, paramName);

							upcomingDeclarator = NextTokenIs(parser, ',');
						}
						else
						{
							// TODO ERROR
							SnekAssert(false, "");
						}
					}
					else
					{
						// TODO ERROR
						SnekAssert(false, "");
					}
				}

				SkipToken(parser, ')');

				AstStatement* body = NULL;

				if (!NextTokenIs(parser, ';'))
				{
					body = ParseStatement(parser);
				}

				AstFunction* decl = CreateAstDeclaration<AstFunction>(parser->module, inputState, DECL_KIND_FUNC);
				decl->returnType = type;
				decl->name = name;
				decl->paramTypes = paramTypes;
				decl->paramNames = paramNames;
				decl->body = body;

				return decl;
			}
		}
	}

	SnekAssert(false, "");
	return NULL;
}

static AstModule* ParseModule(Parser* parser, const SourceFile* file, char* moduleName, int moduleID)
{
	AstModule* ast = CreateAst(moduleName, moduleID);

	parser->module = ast;
	parser->lexer = CreateLexer(file->src, file->moduleName, parser->context);

	while (HasNext(parser))
	{
		AstDeclaration* decl = ParseDeclaration(parser);
		ListAdd(ast->declarations, decl);
	}

	return ast;
}

void ParserRun(Parser* parser)
{
	// TODO multithreading

	for (int i = 0; i < parser->context->sourceFiles.size; i++)
	{
		SourceFile* file = &parser->context->sourceFiles[i];
		parser->context->asts[i] = ParseModule(parser, file, file->moduleName, i);
	}
}
