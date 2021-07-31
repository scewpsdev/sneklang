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

static Token PeekToken(Parser* parser, int offset = 0)
{
	return LexerPeek(parser->lexer, offset);
}

static bool NextTokenIs(Parser* parser, int tokenType, int offset = 0)
{
	return PeekToken(parser, offset).type == tokenType;
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

static AstExpression* ParseExpression(Parser* parser, int prec = 0);

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
		return ParseArgumentOperator(parser, expr);
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
		return ParseArgumentOperator(parser, expr);
	}
	else if (NextTokenIs(parser, '.')) // Dot operator
	{
		NextToken(parser); // .

		char* name = GetTokenString(NextToken(parser));

		auto expr = CreateAstExpression<AstDotOperator>(parser->module, inputState, EXPR_KIND_DOT_OPERATOR);
		expr->operand = expression;
		expr->name = name;
		return ParseArgumentOperator(parser, expr);
	}

	return expression;
}

static AstUnaryOperatorType ParsePrefixOperatorType(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	Token tok = NextToken(parser);
	if (tok.type == TOKEN_TYPE_OP_EXCLAMATION)
		return UNARY_OPERATOR_NOT;
	else if (tok.type == TOKEN_TYPE_OP_MINUS)
		return UNARY_OPERATOR_NEGATE;
	else if (tok.type == TOKEN_TYPE_OP_AMPERSAND)
		return UNARY_OPERATOR_REFERENCE;
	else if (tok.type == TOKEN_TYPE_OP_ASTERISK)
		return UNARY_OPERATOR_DEREFERENCE;
	else if (tok.type == TOKEN_TYPE_OP_PLUS)
	{
		Token tok2 = NextToken(parser);
		if (tok2.type == TOKEN_TYPE_OP_PLUS)
			return UNARY_OPERATOR_INCREMENT;
	}
	else if (tok.type == TOKEN_TYPE_OP_MINUS)
	{
		Token tok2 = NextToken(parser);
		if (tok2.type == TOKEN_TYPE_OP_MINUS)
			return UNARY_OPERATOR_DECREMENT;
	}

	SetInputState(parser, inputState);
	return UNARY_OPERATOR_NULL;
}

static AstExpression* ParsePrefixOperator(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	if (AstUnaryOperatorType operatorType = ParsePrefixOperatorType(parser))
	{
		if (AstExpression* operand = ParseAtom(parser))
		{
			operand = ParseArgumentOperator(parser, operand);

			auto expr = CreateAstExpression<AstUnaryOperator>(parser->module, inputState, EXPR_KIND_UNARY_OPERATOR);
			expr->operand = operand;
			expr->operatorType = operatorType;
			expr->position = false;
			return expr;
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

static AstUnaryOperatorType ParsePostfixOperatorType(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	Token tok = NextToken(parser);
	if (tok.type == TOKEN_TYPE_OP_PLUS)
	{
		Token tok2 = NextToken(parser);
		if (tok2.type == TOKEN_TYPE_OP_PLUS)
			return UNARY_OPERATOR_INCREMENT;
	}
	else if (tok.type == TOKEN_TYPE_OP_MINUS)
	{
		Token tok2 = NextToken(parser);
		if (tok2.type == TOKEN_TYPE_OP_MINUS)
			return UNARY_OPERATOR_DECREMENT;
	}

	SetInputState(parser, inputState);
	return UNARY_OPERATOR_NULL;
}

static AstExpression* ParsePostfixOperator(Parser* parser, AstExpression* expression)
{
	InputState inputState = GetInputState(parser);

	if (AstUnaryOperatorType operatorType = ParsePostfixOperatorType(parser))
	{
		auto expr = CreateAstExpression<AstUnaryOperator>(parser->module, inputState, EXPR_KIND_UNARY_OPERATOR);
		expr->operand = expression;
		expr->operatorType = operatorType;
		expr->position = true;
		return expr;
	}
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

static AstBinaryOperatorType ParseBinaryTernaryOperatorType(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	Token tok = NextToken(parser);
	Token tok2 = PeekToken(parser);
	if (tok.type == TOKEN_TYPE_OP_PLUS)
	{
		if (tok2.type == TOKEN_TYPE_OP_EQUALS)
		{
			NextToken(parser); // =
			return BINARY_OPERATOR_ADD_ASSIGN;
		}
		else
			return BINARY_OPERATOR_ADD;
	}
	else if (tok.type == TOKEN_TYPE_OP_MINUS)
	{
		if (tok2.type == TOKEN_TYPE_OP_EQUALS)
		{
			NextToken(parser); // =
			return BINARY_OPERATOR_SUB_ASSIGN;
		}
		else
			return BINARY_OPERATOR_SUB;
	}
	else if (tok.type == TOKEN_TYPE_OP_ASTERISK)
	{
		if (tok2.type == TOKEN_TYPE_OP_EQUALS)
		{
			NextToken(parser); // =
			return BINARY_OPERATOR_MUL_ASSIGN;
		}
		else
			return BINARY_OPERATOR_MUL;
	}
	else if (tok.type == TOKEN_TYPE_OP_SLASH)
	{
		if (tok2.type == TOKEN_TYPE_OP_EQUALS)
		{
			NextToken(parser); // =
			return BINARY_OPERATOR_DIV_ASSIGN;
		}
		else
			return BINARY_OPERATOR_DIV;
	}
	else if (tok.type == TOKEN_TYPE_OP_PERCENT)
	{
		if (tok2.type == TOKEN_TYPE_OP_EQUALS)
		{
			NextToken(parser); // =
			return BINARY_OPERATOR_MOD_ASSIGN;
		}
		else
			return BINARY_OPERATOR_DIV;
	}
	else if (tok.type == TOKEN_TYPE_OP_EQUALS)
	{
		if (tok2.type == TOKEN_TYPE_OP_EQUALS)
		{
			NextToken(parser); // =
			return BINARY_OPERATOR_EQ;
		}
		else
			return BINARY_OPERATOR_ASSIGN;
	}
	else if (tok.type == TOKEN_TYPE_OP_LESS_THAN)
	{
		if (tok2.type == TOKEN_TYPE_OP_LESS_THAN)
		{
			NextToken(parser); // <
			Token tok3 = PeekToken(parser, 1);
			if (tok3.type == TOKEN_TYPE_OP_EQUALS)
			{
				NextToken(parser); // =
				return BINARY_OPERATOR_BITSHIFT_LEFT_ASSIGN;
			}
			else
				return BINARY_OPERATOR_BITSHIFT_LEFT;
		}
		else
			return BINARY_OPERATOR_LT;
	}
	else if (tok.type == TOKEN_TYPE_OP_GREATER_THAN)
	{
		if (tok2.type == TOKEN_TYPE_OP_GREATER_THAN)
		{
			NextToken(parser); // >
			Token tok3 = PeekToken(parser, 1);
			if (tok3.type == TOKEN_TYPE_OP_EQUALS)
			{
				NextToken(parser); // =
				return BINARY_OPERATOR_BITSHIFT_RIGHT_ASSIGN;
			}
			else
				return BINARY_OPERATOR_BITSHIFT_RIGHT;
		}
		return BINARY_OPERATOR_GT;
	}
	else if (tok.type == TOKEN_TYPE_OP_AMPERSAND)
	{
		if (tok2.type == TOKEN_TYPE_OP_AMPERSAND)
		{
			NextToken(parser); // &
			return BINARY_OPERATOR_AND;
		}
		else if (tok2.type == TOKEN_TYPE_OP_EQUALS)
		{
			NextToken(parser); // =
			return BINARY_OPERATOR_BITWISE_AND_ASSIGN;
		}
		else
			return BINARY_OPERATOR_BITWISE_AND;
	}
	else if (tok.type == TOKEN_TYPE_OP_OR)
	{
		if (tok2.type == TOKEN_TYPE_OP_OR)
		{
			NextToken(parser); // |
			return BINARY_OPERATOR_OR;
		}
		else if (tok2.type == TOKEN_TYPE_OP_EQUALS)
		{
			NextToken(parser); // =
			return BINARY_OPERATOR_BITWISE_OR_ASSIGN;
		}
		else
			return BINARY_OPERATOR_BITWISE_OR;
	}
	else if (tok.type == TOKEN_TYPE_OP_CARET)
	{
		if (tok2.type == TOKEN_TYPE_OP_EQUALS)
		{
			NextToken(parser); // =
			return BINARY_OPERATOR_BITWISE_XOR_ASSIGN;
		}
		else
			return BINARY_OPERATOR_BITWISE_XOR;
	}
	else if (tok.type == TOKEN_TYPE_OP_EXCLAMATION)
	{
		if (tok2.type == TOKEN_TYPE_OP_EQUALS)
		{
			NextToken(parser); // =
			return BINARY_OPERATOR_NE;
		}
	}
	else if (tok.type == TOKEN_TYPE_OP_QUESTION)
	{
		return BINARY_OPERATOR_TERNARY;
	}

	SetInputState(parser, inputState);
	return BINARY_OPERATOR_NULL;
}

static int GetBinaryOperatorPrecedence(AstBinaryOperatorType operatorType)
{
	switch (operatorType)
	{
	case BINARY_OPERATOR_MUL: return 3;
	case BINARY_OPERATOR_DIV: return 3;
	case BINARY_OPERATOR_MOD: return 3;
	case BINARY_OPERATOR_ADD: return 4;
	case BINARY_OPERATOR_SUB: return 4;

	case BINARY_OPERATOR_BITSHIFT_LEFT: return 5;
	case BINARY_OPERATOR_BITSHIFT_RIGHT: return 5;

	case BINARY_OPERATOR_LT: return 6;
	case BINARY_OPERATOR_GT: return 6;
	case BINARY_OPERATOR_LE: return 6;
	case BINARY_OPERATOR_GE: return 6;

	case BINARY_OPERATOR_EQ: return 7;
	case BINARY_OPERATOR_NE: return 7;

	case BINARY_OPERATOR_BITWISE_AND: return 8;
	case BINARY_OPERATOR_BITWISE_XOR: return 9;
	case BINARY_OPERATOR_BITWISE_OR: return 10;
	case BINARY_OPERATOR_AND: return 11;
	case BINARY_OPERATOR_OR: return 12;

	case BINARY_OPERATOR_TERNARY: return 13;

	case BINARY_OPERATOR_ASSIGN: return 14;
	case BINARY_OPERATOR_ADD_ASSIGN: return 14;
	case BINARY_OPERATOR_SUB_ASSIGN: return 14;
	case BINARY_OPERATOR_MUL_ASSIGN: return 14;
	case BINARY_OPERATOR_DIV_ASSIGN: return 14;
	case BINARY_OPERATOR_MOD_ASSIGN: return 14;
	case BINARY_OPERATOR_BITSHIFT_LEFT_ASSIGN: return 14;
	case BINARY_OPERATOR_BITSHIFT_RIGHT_ASSIGN: return 14;
	case BINARY_OPERATOR_BITWISE_AND_ASSIGN: return 14;
	case BINARY_OPERATOR_BITWISE_OR_ASSIGN: return 14;
	case BINARY_OPERATOR_BITWISE_XOR_ASSIGN: return 14;
	case BINARY_OPERATOR_REF_ASSIGN: return 14;

	default: return INT32_MAX;
	}
}

static AstExpression* ParseBinaryTernaryOperator(Parser* parser, AstExpression* expression, int prec = 0)
{
	InputState inputState = GetInputState(parser);

	if (AstBinaryOperatorType operatorType = ParseBinaryTernaryOperatorType(parser))
	{
		int operatorPrec = GetBinaryOperatorPrecedence(operatorType);
		if (operatorPrec < prec)
		{
			AstExpression* result = NULL;
			if (operatorType == BINARY_OPERATOR_TERNARY)
			{
				AstExpression* thenValue = ParseExpression(parser);
				SkipToken(parser, ':');
				AstExpression* elseValue = ParseExpression(parser);

				auto expr = CreateAstExpression<AstTernaryOperator>(parser->module, inputState, EXPR_KIND_TERNARY_OPERATOR);
				expr->condition = expression;
				expr->thenValue = thenValue;
				expr->elseValue = elseValue;
				result = expr;
			}
			else
			{
				AstExpression* left = expression;
				AstExpression* right = ParseExpression(parser, operatorPrec);

				auto expr = CreateAstExpression<AstBinaryOperator>(parser->module, inputState, EXPR_KIND_BINARY_OPERATOR);
				expr->left = left;
				expr->right = right;
				expr->operatorType = operatorType;
				result = expr;
			}
			return ParseBinaryTernaryOperator(parser, result, prec);
		}
		else
		{
			SetInputState(parser, inputState);
		}
	}

	return expression;
}

static AstExpression* ParseExpression(Parser* parser, int prec)
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
