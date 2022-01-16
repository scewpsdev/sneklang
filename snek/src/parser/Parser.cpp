#include "Parser.h"

#include "ast/File.h"
#include "snek.h"
#include "lexer.h"
#include "log.h"
#include "stringbuffer.h"

#include <string.h>
#include <stdlib.h>


Parser* CreateParser(SkContext* context)
{
	Parser* parser = new Parser();

	parser->context = context;
	parser->failed = false;

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

static bool NextTokenIsKeyword(Parser* parser)
{
	Token tok = PeekToken(parser);
	return tok.type == TOKEN_TYPE_IDENTIFIER && tok.keywordType != KEYWORD_TYPE_NULL;
}

static bool NextTokenIsIdentifier(Parser* parser, const char* name)
{
	Token tok = PeekToken(parser);
	return tok.type == TOKEN_TYPE_IDENTIFIER && strlen(name) == tok.len && strncmp(tok.str, name, tok.len) == 0;
}

static void SkipWhitespace(Parser* parser)
{
	PeekToken(parser);
}

static bool SkipToken(Parser* parser, int tokenType)
{
	if (NextTokenIs(parser, tokenType))
	{
		NextToken(parser);
		return true;
	}
	else
	{
		Token tok = NextToken(parser);
		SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_UNEXPECTED_TOKEN, "Expected token '%c'(%d), got '%.*s'", tokenType, tokenType, tok.len, tok.str);
		parser->failed = true;
		return false;
	}
}

static void SkipPastToken(Parser* parser, int tokenType)
{
	while (!NextTokenIs(parser, tokenType) && HasNext(parser))
	{
		NextToken(parser);
	}
	NextToken(parser); // ;
}

static void SkipPastStatement(Parser* parser)
{
	while (!NextTokenIs(parser, ';') && !NextTokenIs(parser, '}') && HasNext(parser))
	{
		NextToken(parser);
	}
	NextToken(parser); // ;
}

static AstExpression* ParseExpression(Parser* parser, int prec = INT32_MAX);

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

		case KEYWORD_TYPE_STRING:
			return CreateAstType<AstStringType>(parser->module, inputState, TYPE_KIND_STRING);

		case KEYWORD_TYPE_NULL:
		{
			auto type = CreateAstType<AstNamedType>(parser->module, inputState, TYPE_KIND_NAMED_TYPE);
			type->name = GetTokenString(tok);
			return type;
		}
		}
	}

	SetInputState(parser, inputState);
	return NULL;
}

static AstType* ParseType(Parser* parser);

static AstType* ParseComplexType(Parser* parser, AstType* elementType)
{
	InputState inputState = elementType->inputState;

	if (NextTokenIs(parser, TOKEN_TYPE_OP_ASTERISK))
	{
		NextToken(parser); // *

		auto pointerType = CreateAstType<AstPointerType>(parser->module, inputState, TYPE_KIND_POINTER);
		pointerType->elementType = elementType;

		return ParseComplexType(parser, pointerType);
	}
	else if (NextTokenIs(parser, '['))
	{
		NextToken(parser); // [
		AstExpression* length = NULL;
		if (!NextTokenIs(parser, ']'))
		{
			length = ParseExpression(parser);
		}
		SkipToken(parser, ']');

		auto arrayType = CreateAstType<AstArrayType>(parser->module, inputState, TYPE_KIND_ARRAY);
		arrayType->elementType = elementType;
		arrayType->length = length;

		return ParseComplexType(parser, arrayType);
	}

	return elementType;
}

static AstType* ParseFunctionType(Parser* parser, AstType* elementType)
{
	InputState inputState = elementType->inputState;

	if (NextTokenIs(parser, '('))
	{
		NextToken(parser); // (

		List<AstType*> paramTypes = CreateList<AstType*>();
		bool varArgs = false;

		bool upcomingParamType = !NextTokenIs(parser, ')');
		while (upcomingParamType)
		{
			if (NextTokenIs(parser, '.'))
			{
				if (SkipToken(parser, '.') && SkipToken(parser, '.') && SkipToken(parser, '.'))
				{
					varArgs = true;
					upcomingParamType = false;
				}
				else
				{
					SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_FUNCTION_SYNTAX, "Variadic arguments need to be declared as '...'");
					parser->failed = true;
				}
			}
			else
			{
				if (AstType* paramType = ParseType(parser))
				{
					paramTypes.add(paramType);

					if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
						NextToken(parser); // skip param names for function types for now

					upcomingParamType = NextTokenIs(parser, ',');
					if (upcomingParamType)
						SkipToken(parser, ',');
				}
				else
				{
					DestroyList(paramTypes);
					SetInputState(parser, inputState);
					return NULL;
				}
			}
		}

		if (!NextTokenIs(parser, ')'))
		{
			DestroyList(paramTypes);
			SetInputState(parser, inputState);
			return NULL;
		}

		SkipToken(parser, ')');

		auto functionType = CreateAstType<AstFunctionType>(parser->module, inputState, TYPE_KIND_FUNCTION);
		functionType->returnType = elementType;
		functionType->paramTypes = paramTypes;
		functionType->varArgs = varArgs;

		return ParseComplexType(parser, functionType);
	}

	return elementType;
}

static AstType* ParseType(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	if (AstType* elementType = ParseElementType(parser))
	{
		return ParseFunctionType(parser, ParseComplexType(parser, elementType));
	}

	return NULL;
}

static AstExpression* ParseAtom(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	if (NextTokenIs(parser, TOKEN_TYPE_INT_LITERAL))
	{
		Token token = NextToken(parser);
		char* str = GetTokenString(token);
		int64_t value = strtoll(str, NULL, 0);
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
					SnekWarn(parser->context, parser->lexer->input.state, ERROR_CODE_INVALID_LITERAL, "Undefined escape character '\\%c'", tok.str[i]);
					break;
				}
			}
			value = c;

			if (i < tok.len - 1) {
				SnekWarn(parser->context, parser->lexer->input.state, ERROR_CODE_INVALID_LITERAL, "Invalid character literal length: %d", tok.len - i);
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
	else if (NextTokenIs(parser, TOKEN_TYPE_STRING_LITERAL))
	{
		Token token = NextToken(parser);
		StringBuffer buffer = CreateStringBuffer(8);
		for (int i = 0; i < token.len; i++)
		{
			char c = token.str[i];
			if (c == '\\')
			{
				switch (token.str[++i])
				{
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 't': c = '\t'; break;
				case '\\': c = '\\'; break;
				case '0': c = '\0'; break;
				default:
					SnekWarn(parser->context, parser->lexer->input.state, ERROR_CODE_INVALID_LITERAL, "Undefined escape character '\\%c'", token.str[i]);
					break;
				}
			}
			buffer << c;
		}

		auto expr = CreateAstExpression<AstStringLiteral>(parser->module, inputState, EXPR_KIND_STRING_LITERAL);
		expr->value = buffer.buffer;
		expr->length = buffer.length;
		return expr;
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
			else
			{
				Token tok = NextToken(parser);
				SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_UNEXPECTED_TOKEN, "Expected ')': %.*s", tok.len, tok.str);
				parser->failed = true;
				return NULL;
			}
		}
		else
		{
			Token tok = NextToken(parser);
			SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_EXPRESSION_EXPECTED, "Expected an expression: %.*s", tok.len, tok.str);
			parser->failed = true;
			return NULL;
		}
	}

	return NULL;
}

static AstExpression* ParseArgumentOperator(Parser* parser, AstExpression* expression)
{
	InputState inputState = GetInputState(parser);

	if (NextTokenIs(parser, '(') || NextTokenIs(parser, TOKEN_TYPE_OP_LESS_THAN)) // Function call
	{
		NextToken(parser); // (

		List<AstExpression*> arguments = CreateList<AstExpression*>();

		bool isGenericCall = false;
		List<AstType*> genericArgs = CreateList<AstType*>();

		bool upcomingType = !NextTokenIs(parser, TOKEN_TYPE_OP_LESS_THAN);
		while (HasNext(parser) && upcomingType)
		{
			isGenericCall = true;
			if (AstType* genericArg = ParseType(parser))
			{
				genericArgs.add(genericArg);

				upcomingType = NextTokenIs(parser, ',');
				if (upcomingType)
					SkipToken(parser, ',');
			}
			else
			{
				SnekAssert(false);
			}
		}

		bool upcomingDeclarator = !NextTokenIs(parser, ')');
		while (HasNext(parser) && upcomingDeclarator)
		{
			if (AstExpression* argument = ParseExpression(parser))
			{
				arguments.add(argument);

				upcomingDeclarator = NextTokenIs(parser, ',');
				if (upcomingDeclarator)
					SkipToken(parser, ',');
			}
			else
			{
				SnekAssert(false);
			}
		}

		SkipToken(parser, ')');

		auto expr = CreateAstExpression<AstFuncCall>(parser->module, inputState, EXPR_KIND_FUNC_CALL);
		expr->calleeExpr = expression;
		expr->arguments = arguments;
		expr->isGenericCall = isGenericCall;
		expr->genericArgs = genericArgs;
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
				arguments.add(argument);

				upcomingDeclarator = NextTokenIs(parser, ',');
			}
			else
			{
				Token tok = NextToken(parser);
				SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_EXPRESSION_EXPECTED, "Expected an expression: %.*s", tok.len, tok.str);
				parser->failed = true;
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

	if (AstType* type = ParseElementType(parser))
	{
		if (NextTokenIs(parser, '{'))
		{
			NextToken(parser); // {

			List<AstExpression*> values = CreateList<AstExpression*>();

			bool upcomingValue = !NextTokenIs(parser, '}');
			while (upcomingValue && HasNext(parser))
			{
				AstExpression* value = ParseExpression(parser);
				values.add(value);

				upcomingValue = NextTokenIs(parser, ',');
				if (upcomingValue)
					NextToken(parser); // ,
			}

			SkipToken(parser, '}');

			auto expr = CreateAstExpression<AstStructLiteral>(parser->module, inputState, EXPR_KIND_STRUCT_LITERAL);
			expr->structType = type;
			expr->values = values;
			return expr;
		}
		else
		{
			SetInputState(parser, inputState);
		}
	}
	if (NextTokenIs(parser, '('))
	{
		NextToken(parser); // (
		if (AstType* castType = ParseType(parser)) // Cast
		{
			if (NextTokenIs(parser, ')'))
			{
				NextToken(parser); // )
				if (AstExpression* value = ParsePrefixOperator(parser))
				{
					if (value = ParsePostfixOperator(parser, value))
					{
						auto expr = CreateAstExpression<AstCast>(parser->module, inputState, EXPR_KIND_CAST);
						expr->value = value;
						expr->castType = castType;
						return expr;
					}
				}
			}
		}
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_SIZEOF))
	{
		NextToken(parser); // sizeof

		AstType* sizedType = ParseType(parser);

		auto expr = CreateAstExpression<AstSizeof>(parser->module, inputState, EXPR_KIND_SIZEOF);
		expr->sizedType = sizedType;
		return expr;
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_MALLOC))
	{
		NextToken(parser); // malloc

		AstType* type = ParseComplexType(parser, ParseElementType(parser));
		AstExpression* count = NULL;
		bool hasArguments = false;
		List<AstExpression*> arguments = CreateList<AstExpression*>();

		if (NextTokenIs(parser, '('))
		{
			NextToken(parser); // (

			hasArguments = true;

			bool upcomingDeclarator = !NextTokenIs(parser, ')');
			while (HasNext(parser) && upcomingDeclarator)
			{
				if (AstExpression* argument = ParseExpression(parser))
				{
					arguments.add(argument);

					upcomingDeclarator = NextTokenIs(parser, ',');
					if (upcomingDeclarator)
						SkipToken(parser, ',');
				}
				else
				{
					Token tok = NextToken(parser);
					SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_EXPRESSION_EXPECTED, "Expected an expression: %.*s", tok.len, tok.str);
					parser->failed = true;
				}
			}

			SkipToken(parser, ')');
		}

		if (NextTokenIs(parser, ':'))
		{
			NextToken(parser); // :
			count = ParseExpression(parser);
		}

		auto expr = CreateAstExpression<AstMalloc>(parser->module, inputState, EXPR_KIND_MALLOC);
		expr->mallocType = type;
		expr->count = count;
		expr->hasArguments = hasArguments;
		expr->arguments = arguments;
		return expr;
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
			return BINARY_OPERATOR_MOD;
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
			{
				return BINARY_OPERATOR_BITSHIFT_LEFT;
			}
		}
		else if (tok2.type == TOKEN_TYPE_OP_EQUALS)
		{
			NextToken(parser); // =
			return BINARY_OPERATOR_LE;
		}
		else
		{
			return BINARY_OPERATOR_LT;
		}
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
			{
				return BINARY_OPERATOR_BITSHIFT_RIGHT;
			}
		}
		else if (tok2.type == TOKEN_TYPE_OP_EQUALS)
		{
			NextToken(parser); // =
			return BINARY_OPERATOR_GE;
		}
		else
		{
			return BINARY_OPERATOR_GT;
		}
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

static AstExpression* ParseBinaryTernaryOperator(Parser* parser, AstExpression* expression, int prec = INT32_MAX)
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
		expr = ParseBinaryTernaryOperator(parser, expr, prec);
		return expr;
	}

	Token tok = NextToken(parser);
	SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_EXPRESSION_EXPECTED, "Expected an expression: %.*s", tok.len, tok.str);
	SkipPastStatement(parser);
	parser->failed = true;
	return NULL;
}

static AstStatement* ParseStatement(Parser* parser)
{
	SkipWhitespace(parser);
	InputState inputState = GetInputState(parser);

	if (NextTokenIs(parser, ';'))
	{
		NextToken(parser); // ;

		AstNoOpStatement* statement = CreateAstStatement<AstNoOpStatement>(parser->module, inputState, STATEMENT_KIND_NO_OP);
		return statement;
	}
	else if (NextTokenIs(parser, '{'))
	{
		NextToken(parser); // {

		List<AstStatement*> statements = CreateList<AstStatement*>();

		while (HasNext(parser) && !NextTokenIs(parser, '}'))
		{
			AstStatement* statement = ParseStatement(parser);
			statements.add(statement);
		}

		NextToken(parser); // }

		AstCompoundStatement* statement = CreateAstStatement<AstCompoundStatement>(parser->module, inputState, STATEMENT_KIND_COMPOUND);
		statement->statements = statements;
		return statement;
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_IF))
	{
		NextToken(parser); // if

		SkipToken(parser, '(');
		AstExpression* condition = ParseExpression(parser);
		SkipToken(parser, ')');

		AstStatement* thenStatement = ParseStatement(parser);
		AstStatement* elseStatement = NULL;

		if (NextTokenIsKeyword(parser, KEYWORD_TYPE_ELSE))
		{
			NextToken(parser); // else
			elseStatement = ParseStatement(parser);
		}

		auto statement = CreateAstStatement<AstIfStatement>(parser->module, inputState, STATEMENT_KIND_IF);
		statement->condition = condition;
		statement->thenStatement = thenStatement;
		statement->elseStatement = elseStatement;
		return statement;
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_WHILE))
	{
		NextToken(parser); // while

		SkipToken(parser, '(');
		AstExpression* condition = ParseExpression(parser);
		SkipToken(parser, ')');

		AstStatement* body = ParseStatement(parser);

		auto statement = CreateAstStatement<AstWhileLoop>(parser->module, inputState, STATEMENT_KIND_WHILE);
		statement->condition = condition;
		statement->body = body;
		return statement;
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_FOR))
	{
		NextToken(parser); // for

		if (SkipToken(parser, '('))
		{
			if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
			{
				char* iteratorName = GetTokenString(NextToken(parser));
				SkipToken(parser, ',');
				AstExpression* startValue = ParseExpression(parser);
				SkipToken(parser, ',');
				AstExpression* endValue = ParseExpression(parser);
				AstExpression* deltaValue = NULL;
				if (NextTokenIs(parser, ','))
				{
					NextToken(parser); // ,
					deltaValue = ParseExpression(parser);
				}
				SkipToken(parser, ')');

				AstStatement* body = ParseStatement(parser);

				auto statement = CreateAstStatement<AstForLoop>(parser->module, inputState, STATEMENT_KIND_FOR);
				statement->iteratorName = iteratorName;
				statement->startValue = startValue;
				statement->endValue = endValue;
				statement->deltaValue = deltaValue;
				statement->body = body;
				return statement;
			}
			else
			{
				SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_FOR_LOOP_SYNTAX, "Expected an iterator name after 'for'");
				parser->failed = true;
			}
		}
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_BREAK))
	{
		NextToken(parser); // break
		SkipToken(parser, ';');

		auto statement = CreateAstStatement<AstBreak>(parser->module, inputState, STATEMENT_KIND_BREAK);
		return statement;
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_CONTINUE))
	{
		NextToken(parser); // continue
		SkipToken(parser, ';');

		auto statement = CreateAstStatement<AstContinue>(parser->module, inputState, STATEMENT_KIND_CONTINUE);
		return statement;
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_RETURN))
	{
		NextToken(parser); // return

		AstExpression* value = NULL;
		if (!NextTokenIs(parser, ';'))
			value = ParseExpression(parser);

		SkipToken(parser, ';');

		auto statement = CreateAstStatement<AstReturn>(parser->module, inputState, STATEMENT_KIND_RETURN);
		statement->value = value;
		return statement;
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_FREE))
	{
		NextToken(parser); // free

		List<AstExpression*> values = CreateList<AstExpression*>();

		bool upcomingValue = true;
		while (HasNext(parser) && upcomingValue)
		{
			AstExpression* value = ParseExpression(parser);
			values.add(value);

			upcomingValue = NextTokenIs(parser, ',');
			if (upcomingValue)
				SkipToken(parser, ',');
		}

		SkipToken(parser, ';');

		auto statement = CreateAstStatement<AstFree>(parser->module, inputState, STATEMENT_KIND_FREE);
		statement->values = values;
		return statement;
	}
	else if (AstType* type = ParseType(parser))
	{
		if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
		{
			List<AstVarDeclarator> declarators = CreateList<AstVarDeclarator>();

			bool upcomingDeclarator = true;
			while (HasNext(parser) && upcomingDeclarator)
			{
				InputState inputState = GetInputState(parser);

				char* name = GetTokenString(NextToken(parser));
				AstExpression* value = NULL;

				if (NextTokenIs(parser, TOKEN_TYPE_OP_EQUALS))
				{
					NextToken(parser); // =
					value = ParseExpression(parser);
				}

				upcomingDeclarator = NextTokenIs(parser, ',');
				if (upcomingDeclarator)
					SkipToken(parser, ',');

				AstVarDeclarator declarator = {};
				declarator.module = parser->module;
				declarator.inputState = inputState;
				declarator.name = name;
				declarator.value = value;

				declarators.add(declarator);
			}

			SkipToken(parser, ';');

			AstVarDeclStatement* statement = CreateAstStatement<AstVarDeclStatement>(parser->module, inputState, STATEMENT_KIND_VAR_DECL);
			statement->type = type;
			statement->isConstant = false;
			statement->declarators = declarators;

			return statement;
		}
		else
		{
			SetInputState(parser, inputState);
		}
	}
	if (AstExpression* expression = ParseExpression(parser))
	{
		SkipToken(parser, ';');

		AstExprStatement* statement = CreateAstStatement<AstExprStatement>(parser->module, inputState, STATEMENT_KIND_EXPR);
		statement->expr = expression;

		return statement;
	}

	SkipPastStatement(parser);
	return NULL;
}

static AstDeclaration* ParseDeclaration(Parser* parser)
{
	InputState inputState = GetInputState(parser);

	uint32_t flags = DECL_FLAG_NONE;

	while (NextTokenIsKeyword(parser) && HasNext(parser))
	{
		if (NextTokenIsKeyword(parser, KEYWORD_TYPE_EXTERN))
		{
			NextToken(parser); // extern
			flags |= DECL_FLAG_EXTERN;
		}
		else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_DLLEXPORT))
		{
			NextToken(parser); // dllexport
			flags |= DECL_FLAG_LINKAGE_DLLEXPORT;
		}
		else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_DLLIMPORT))
		{
			NextToken(parser); // dllimport
			flags |= DECL_FLAG_LINKAGE_DLLIMPORT;
		}
		else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_CONSTANT))
		{
			NextToken(parser); // const
			flags |= DECL_FLAG_CONSTANT;
		}
		else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_PUBLIC))
		{
			NextToken(parser); // public
			flags |= DECL_FLAG_VISIBILITY_PUBLIC;
		}
		else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_PRIVATE))
		{
			NextToken(parser); // private
			flags |= DECL_FLAG_VISIBILITY_PRIVATE;
		}
		else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_PACKED))
		{
			NextToken(parser); // packed
			flags |= DECL_FLAG_PACKED;
		}
		else
		{
			break;
		}
	}

	if (NextTokenIsKeyword(parser, KEYWORD_TYPE_STRUCT))
	{
		NextToken(parser); // struct
		if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
		{
			char* name = GetTokenString(NextToken(parser));
			bool hasBody = false;
			List<AstStructField> fields = CreateList<AstStructField>();

			if (NextTokenIs(parser, '{'))
			{
				NextToken(parser); // {

				hasBody = true;

				bool upcomingMember = !NextTokenIs(parser, '}');
				while (HasNext(parser) && upcomingMember)
				{
					InputState fieldInputState = GetInputState(parser);
					if (AstType* type = ParseType(parser))
					{
						bool upcomingField = true;

						while (upcomingField && HasNext(parser))
						{
							if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
							{
								char* name = GetTokenString(NextToken(parser));
								AstStructField field = {};
								field.inputState = fieldInputState;
								field.type = type;
								field.name = name;
								field.index = fields.size;
								fields.add(field);
							}
							else
							{
								Token tok = NextToken(parser);
								SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_STRUCT_SYNTAX, "Expected a field name: %.*s", tok.len, tok.str);
								SkipPastToken(parser, ';');
								parser->failed = true;
								break;
							}

							upcomingField = NextTokenIs(parser, ',');
							if (upcomingField)
							{
								NextToken(parser); // ,
								type = CopyType(type, parser->module);
							}
						}
					}
					else
					{
						Token tok = NextToken(parser);
						SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_STRUCT_SYNTAX, "Expected a type: %.*s", tok.len, tok.str);
						SkipPastToken(parser, ';');
						parser->failed = true;
					}

					SkipToken(parser, ';');

					upcomingMember = !NextTokenIs(parser, '}');
				}
				SkipToken(parser, '}');
			}
			else
			{
				SkipToken(parser, ';');
			}

			auto decl = CreateAstDeclaration<AstStruct>(parser->module, inputState, DECL_KIND_STRUCT);
			decl->flags = flags;
			decl->name = name;
			decl->hasBody = hasBody;
			decl->fields = fields;
			return decl;
		}
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_CLASS))
	{
		NextToken(parser); // class
		if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
		{
			char* className = GetTokenString(NextToken(parser));
			List<AstClassField> fields = CreateList<AstClassField>();
			List<AstFunction*> methods = CreateList<AstFunction*>();
			AstFunction* constructor = NULL;

			if (NextTokenIs(parser, '{'))
			{
				NextToken(parser); // {

				bool upcomingMember = !NextTokenIs(parser, '}');
				while (HasNext(parser) && upcomingMember)
				{
					InputState memberInputState = GetInputState(parser);

					if (AstType* type = ParseType(parser))
					{
						if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
						{
							char* name = GetTokenString(NextToken(parser));

							if (NextTokenIs(parser, '('))
							{
								NextToken(parser); // (

								List<AstType*> paramTypes;
								List<char*> paramNames;
								bool varArgs = false;

								bool upcomingDeclarator = !NextTokenIs(parser, ')');
								while (HasNext(parser) && upcomingDeclarator)
								{
									if (NextTokenIs(parser, '.'))
									{
										if (SkipToken(parser, '.') && SkipToken(parser, '.') && SkipToken(parser, '.'))
										{
											varArgs = true;
											upcomingDeclarator = false;
										}
										else
										{
											SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_FUNCTION_SYNTAX, "Variadic arguments need to be declared as '...'");
											parser->failed = true;
										}
									}
									else if (AstType* paramType = ParseType(parser))
									{
										if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
										{
											char* paramName = GetTokenString(NextToken(parser));

											paramTypes.add(paramType);
											paramNames.add(paramName);

											upcomingDeclarator = NextTokenIs(parser, ',');
											if (upcomingDeclarator)
												SkipToken(parser, ',');
										}
										else
										{
											SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_FUNCTION_SYNTAX, "Function parameter declaration needs a name");
											parser->failed = true;
										}
									}
									else
									{
										Token tok = NextToken(parser);
										SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_UNEXPECTED_TOKEN, "Unexpected token: %.*s", tok.len, tok.str);
										parser->failed = true;
									}
								}

								SkipToken(parser, ')');

								AstStatement* body = NULL;

								if (NextTokenIs(parser, ';'))
								{
									NextToken(parser); // ;
								}
								else
								{
									body = ParseStatement(parser);
								}

								InputState endInputState = GetInputState(parser);

								AstFunction* method = CreateAstDeclaration<AstFunction>(parser->module, inputState, DECL_KIND_CLASS_METHOD);
								method->name = name;
								method->returnType = type;
								method->paramTypes = paramTypes;
								method->paramNames = paramNames;
								method->varArgs = varArgs;
								method->body = body;
								method->endInputState = endInputState;

								methods.add(method);
							}
							else
							{
								bool upcomingField = true;

								while (upcomingField && HasNext(parser))
								{
									AstClassField field = {};
									field.inputState = memberInputState;
									field.type = type;
									field.name = name;
									field.index = fields.size;
									fields.add(field);

									upcomingField = NextTokenIs(parser, ',');
									if (upcomingField)
									{
										NextToken(parser); // ,
										name = GetTokenString(NextToken(parser));
										type = CopyType(type, parser->module);
									}
								}

								SkipToken(parser, ';');
							}
						}
						else
						{
							if (type->typeKind == TYPE_KIND_FUNCTION && ((AstFunctionType*)type)->returnType->typeKind == TYPE_KIND_NAMED_TYPE && strcmp(((AstNamedType*)((AstFunctionType*)type)->returnType)->name, className) == 0)
							{
								SetInputState(parser, memberInputState);

								// Constructor declaration

								NextToken(parser); // class name
								SkipToken(parser, '(');

								List<AstType*> paramTypes;
								List<char*> paramNames;
								bool varArgs = false;

								bool upcomingDeclarator = !NextTokenIs(parser, ')');
								while (HasNext(parser) && upcomingDeclarator)
								{
									if (NextTokenIs(parser, '.'))
									{
										if (SkipToken(parser, '.') && SkipToken(parser, '.') && SkipToken(parser, '.'))
										{
											varArgs = true;
											upcomingDeclarator = false;
										}
										else
										{
											SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_FUNCTION_SYNTAX, "Variadic arguments need to be declared as '...'");
											parser->failed = true;
										}
									}
									else if (AstType* paramType = ParseType(parser))
									{
										if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
										{
											char* paramName = GetTokenString(NextToken(parser));

											paramTypes.add(paramType);
											paramNames.add(paramName);

											upcomingDeclarator = NextTokenIs(parser, ',');
											if (upcomingDeclarator)
												SkipToken(parser, ',');
										}
										else
										{
											SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_FUNCTION_SYNTAX, "Function parameter declaration needs a name");
											parser->failed = true;
										}
									}
									else
									{
										Token tok = NextToken(parser);
										SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_UNEXPECTED_TOKEN, "Unexpected token: %.*s", tok.len, tok.str);
										parser->failed = true;
									}
								}

								SkipToken(parser, ')');

								AstStatement* body = NULL;

								if (NextTokenIs(parser, ';'))
								{
									NextToken(parser); // ;
								}
								else
								{
									body = ParseStatement(parser);
								}

								InputState endInputState = GetInputState(parser);

								auto constructorReturnType = CreateAstType<AstNamedType>(parser->module, inputState, TYPE_KIND_NAMED_TYPE);
								constructorReturnType->name = className;

								constructor = CreateAstDeclaration<AstFunction>(parser->module, inputState, DECL_KIND_CLASS_CONSTRUCTOR);
								constructor->returnType = constructorReturnType;
								constructor->paramTypes = paramTypes;
								constructor->paramNames = paramNames;
								constructor->varArgs = varArgs;
								constructor->body = body;
								constructor->endInputState = endInputState;
							}
							else
							{
								Token tok = NextToken(parser);
								SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_CLASS_SYNTAX, "Expected a field name: %.*s", tok.len, tok.str);
								SkipPastToken(parser, ';');
								parser->failed = true;
							}
						}
					}
					else
					{
						Token tok = NextToken(parser);
						SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_CLASS_SYNTAX, "Expected a type: %.*s", tok.len, tok.str);
						SkipPastToken(parser, ';');
						parser->failed = true;
					}

					upcomingMember = !NextTokenIs(parser, '}');
				}
				SkipToken(parser, '}');
			}
			else
			{
				SkipToken(parser, ';');
			}

			auto decl = CreateAstDeclaration<AstClass>(parser->module, inputState, DECL_KIND_CLASS);
			decl->flags = flags;
			decl->name = className;
			decl->fields = fields;
			decl->methods = methods;
			decl->constructor = constructor;

			return decl;
		}
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_TYPEDEF))
	{
		NextToken(parser); // typedef

		char* name = GetTokenString(NextToken(parser));
		if (SkipToken(parser, ':'))
		{
			AstType* alias = ParseType(parser);

			SkipToken(parser, ';');

			auto decl = CreateAstDeclaration<AstTypedef>(parser->module, inputState, DECL_KIND_TYPEDEF);
			decl->flags = flags;
			decl->name = name;
			decl->alias = alias;

			return decl;
		}
		else
		{
			SkipPastToken(parser, ';');
			return NULL;
		}
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_ENUM))
	{
		NextToken(parser); // enum

		char* name = GetTokenString(NextToken(parser));
		AstType* alias = NULL;
		List<AstEnumValue> values = CreateList<AstEnumValue>();

		if (NextTokenIs(parser, ':'))
		{
			NextToken(parser); // :
			alias = ParseType(parser);
		}

		SkipToken(parser, '{');

		bool upcomingEntry = NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER);

		while (upcomingEntry && HasNext(parser))
		{
			if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
			{
				char* entryName = GetTokenString(NextToken(parser));
				AstExpression* entryValue = NULL;
				if (NextTokenIs(parser, TOKEN_TYPE_OP_EQUALS))
				{
					NextToken(parser); // =
					entryValue = ParseExpression(parser);
				}

				AstEnumValue value;
				value.name = entryName;
				value.value = entryValue;

				values.add(value);

				upcomingEntry = NextTokenIs(parser, ',') && !NextTokenIs(parser, '}', 1);
				if (NextTokenIs(parser, ','))
					NextToken(parser); // ,
			}
			else
			{
				// ERROR
			}
		}

		SkipToken(parser, '}');

		auto decl = CreateAstDeclaration<AstEnum>(parser->module, inputState, DECL_KIND_ENUM);
		decl->flags = flags;
		decl->name = name;
		decl->alias = alias;
		decl->values = values;

		return decl;
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_EXPRDEF))
	{
		NextToken(parser); // exprdef

		char* name = GetTokenString(NextToken(parser));
		if (SkipToken(parser, TOKEN_TYPE_OP_EQUALS))
		{
			AstExpression* expr = ParseExpression(parser);

			SkipToken(parser, ';');

			auto decl = CreateAstDeclaration<AstExprdef>(parser->module, inputState, DECL_KIND_EXPRDEF);
			decl->flags = flags;
			decl->name = name;
			decl->alias = expr;

			return decl;
		}
		else
		{
			SkipPastToken(parser, ';');
			return NULL;
		}
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_MODULE))
	{
		NextToken(parser); // module
		if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
		{
			List<char*> namespaces = CreateList<char*>();

			bool upcomingNamespace = true;
			while (upcomingNamespace && HasNext(parser))
			{
				char* name = GetTokenString(NextToken(parser));
				namespaces.add(name);

				upcomingNamespace = NextTokenIs(parser, '.');
				if (upcomingNamespace)
					NextToken(parser); // .
			}
			SkipToken(parser, ';');

			AstModuleDecl* decl = CreateAstDeclaration<AstModuleDecl>(parser->module, inputState, DECL_KIND_MODULE);
			decl->namespaces = namespaces;
			return decl;
		}
		else
		{
			Token tok = NextToken(parser);
			SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_MODULE_SYNTAX, "Expected a module name: %.*s", tok.len, tok.str);
			parser->failed = true;
			return NULL;
		}
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_NAMESPACE))
	{
		NextToken(parser); // namespace
		char* nameSpace = GetTokenString(NextToken(parser));
		SkipToken(parser, ';');

		AstNamespaceDecl* decl = CreateAstDeclaration<AstNamespaceDecl>(parser->module, inputState, DECL_KIND_NAMESPACE);
		decl->name = nameSpace;
		return decl;
	}
	else if (NextTokenIsKeyword(parser, KEYWORD_TYPE_IMPORT))
	{
		NextToken(parser); // import
		if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
		{
			List<List<char*>> imports = CreateList<List<char*>>();

			bool upcomingImport = true;
			while (upcomingImport && HasNext(parser))
			{
				List<char*> names = CreateList<char*>();

				bool upcomingModule = true;
				while (upcomingModule && HasNext(parser))
				{
					if (!(NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER) || NextTokenIs(parser, TOKEN_TYPE_OP_ASTERISK)))
					{
						SkipPastToken(parser, ';');
						break;
					}

					char* name = GetTokenString(NextToken(parser));
					names.add(name);

					upcomingModule = NextTokenIs(parser, '.');
					if (upcomingModule)
						NextToken(parser); // .
				}

				imports.add(names);

				upcomingImport = NextTokenIs(parser, ',');
				if (upcomingImport)
					NextToken(parser); // ,
			}
			SkipToken(parser, ';');

			AstImport* decl = CreateAstDeclaration<AstImport>(parser->module, inputState, DECL_KIND_IMPORT);
			decl->flags = flags;
			decl->imports = imports;
			//decl->modules = CreateList<AstModule*>();
			//decl->modules.reserve(names.size);
			return decl;
		}
		else
		{
			Token tok = NextToken(parser);
			SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_IMPORT_SYNTAX, "Expected a module name: %.*s", tok.len, tok.str);
			parser->failed = true;
			return NULL;
		}
	}
	else if (AstType* type = ParseType(parser))
	{
		if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
		{
			char* name = GetTokenString(NextToken(parser));
			bool isGeneric = false;
			List<char*> genericParams;

			if (NextTokenIs(parser, TOKEN_TYPE_OP_LESS_THAN)) // Generic types
			{
				NextToken(parser); // <

				isGeneric = true;
				genericParams = CreateList<char*>();

				bool hasNext = !NextTokenIs(parser, TOKEN_TYPE_OP_GREATER_THAN);
				while (hasNext)
				{
					if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
					{
						char* genericParamName = GetTokenString(NextToken(parser));
						genericParams.add(genericParamName);

						hasNext = NextTokenIs(parser, ',');
						if (hasNext)
							NextToken(parser); // ,
					}
					else
					{
						SnekAssert(false); // TODO ERROR
					}
				}

				SkipToken(parser, TOKEN_TYPE_OP_GREATER_THAN);
			}

			if (NextTokenIs(parser, '(')) // Function declaration
			{
				NextToken(parser); // (

				List<AstType*> paramTypes;
				List<char*> paramNames;
				bool varArgs = false;

				bool upcomingDeclarator = !NextTokenIs(parser, ')');
				while (HasNext(parser) && upcomingDeclarator)
				{
					if (NextTokenIs(parser, '.'))
					{
						if (SkipToken(parser, '.') && SkipToken(parser, '.') && SkipToken(parser, '.'))
						{
							varArgs = true;
							upcomingDeclarator = false;
						}
						else
						{
							SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_FUNCTION_SYNTAX, "Variadic arguments need to be declared as '...'");
							parser->failed = true;
						}
					}
					else if (AstType* paramType = ParseType(parser))
					{
						if (NextTokenIs(parser, TOKEN_TYPE_IDENTIFIER))
						{
							char* paramName = GetTokenString(NextToken(parser));

							paramTypes.add(paramType);
							paramNames.add(paramName);

							upcomingDeclarator = NextTokenIs(parser, ',');
							if (upcomingDeclarator)
								SkipToken(parser, ',');
						}
						else
						{
							SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_FUNCTION_SYNTAX, "Function parameter declaration needs a name");
							parser->failed = true;
						}
					}
					else
					{
						Token tok = NextToken(parser);
						SnekError(parser->context, parser->lexer->input.state, ERROR_CODE_UNEXPECTED_TOKEN, "Unexpected token: '%.*s'", tok.len, tok.str);
						parser->failed = true;
					}
				}

				SkipToken(parser, ')');

				AstStatement* body = NULL;

				if (NextTokenIs(parser, ';'))
				{
					NextToken(parser); // ;
				}
				else
				{
					body = ParseStatement(parser);
				}

				InputState endInputState = GetInputState(parser);

				AstFunction* decl = CreateAstDeclaration<AstFunction>(parser->module, inputState, DECL_KIND_FUNC);

				decl->flags = flags;
				decl->name = name;
				decl->returnType = type;
				decl->paramTypes = paramTypes;
				decl->paramNames = paramNames;
				decl->varArgs = varArgs;
				decl->body = body;
				decl->endInputState = endInputState;

				decl->isGeneric = isGeneric;
				decl->genericParams = genericParams;

				return decl;
			}
			else
			{
				// Global variable declaration

				AstExpression* value = NULL;

				if (NextTokenIs(parser, TOKEN_TYPE_OP_EQUALS))
				{
					NextToken(parser); // =
					value = ParseExpression(parser);
				}

				SkipToken(parser, ';');

				AstGlobal* decl = CreateAstDeclaration<AstGlobal>(parser->module, inputState, DECL_KIND_GLOBAL);
				decl->flags = flags;
				decl->type = type;
				decl->name = name;
				decl->value = value;

				return decl;
			}
		}
	}

	Token token = NextToken(parser);
	SnekError(parser->context, inputState, ERROR_CODE_UNEXPECTED_TOKEN, "Unexpected token '%.*s", token.len, token.str);
	parser->failed = true;

	return NULL;
}

static AstFile* ParseModule(Parser* parser, SourceFile* file, char* moduleName, int moduleID)
{
	AstFile* ast = CreateAst(moduleName, moduleID, file);

	parser->module = ast;
	parser->lexer = CreateLexer(file->src, file->filename, parser->context);

	while (HasNext(parser))
	{
		if (AstDeclaration* decl = ParseDeclaration(parser))
		{
			switch (decl->declKind)
			{
			case DECL_KIND_FUNC:
				ast->functions.add((AstFunction*)decl);
				break;
			case DECL_KIND_STRUCT:
				ast->structs.add((AstStruct*)decl);
				break;
			case DECL_KIND_CLASS:
				ast->classes.add((AstClass*)decl);
				break;
			case DECL_KIND_TYPEDEF:
				ast->typedefs.add((AstTypedef*)decl);
				break;
			case DECL_KIND_ENUM:
				ast->enums.add((AstEnum*)decl);
				break;
			case DECL_KIND_EXPRDEF:
				ast->exprdefs.add((AstExprdef*)decl);
				break;
			case DECL_KIND_GLOBAL:
				ast->globals.add((AstGlobal*)decl);
				break;
			case DECL_KIND_MODULE:
				ast->moduleDecl = (AstModuleDecl*)decl;
				break;
			case DECL_KIND_NAMESPACE:
				ast->namespaceDecl = (AstNamespaceDecl*)decl;
				break;
			case DECL_KIND_IMPORT:
				ast->imports.add((AstImport*)decl);
				break;
			default:
				SnekAssert(false);
				break;
			}
		}
	}

	return ast;
}

bool ParserRun(Parser* parser)
{
	// TODO multithreading
	parser->failed = false;

	for (int i = 0; i < parser->context->sourceFiles.size; i++)
	{
		SourceFile* file = &parser->context->sourceFiles[i];
		parser->context->asts[i] = ParseModule(parser, file, file->moduleName, i);
	}

	return !parser->failed;
}
