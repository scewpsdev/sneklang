#include "Lexer.h"

#include "Keywords.h"
#include "utils/Log.h"

#include <assert.h>
#include <ctype.h>
#include <map>
#include <string>


static const Token NULL_TOKEN = {};

static const std::map<std::string, KeywordType> keywords =
{
	{ KEYWORD_MODULE, KEYWORD_TYPE_MODULE },
	{ KEYWORD_NAMESPACE, KEYWORD_TYPE_NAMESPACE },
	{ KEYWORD_IMPORT, KEYWORD_TYPE_IMPORT },
	{ KEYWORD_VAR, KEYWORD_TYPE_VARIABLE },
	{ KEYWORD_FUNC, KEYWORD_TYPE_FUNCTION },
	{ KEYWORD_STRUCT, KEYWORD_TYPE_STRUCT },
	{ KEYWORD_CLASS, KEYWORD_TYPE_CLASS },
	{ KEYWORD_TYPEDEF, KEYWORD_TYPE_TYPEDEF },
	{ KEYWORD_EXPRDEF, KEYWORD_TYPE_EXPRDEF },
	{ KEYWORD_METHOD, KEYWORD_TYPE_METHOD },
	{ KEYWORD_ENUM, KEYWORD_TYPE_ENUM },

	{ KEYWORD_PUBLIC, KEYWORD_TYPE_PUBLIC },
	{ KEYWORD_PRIVATE, KEYWORD_TYPE_PRIVATE },
	{ KEYWORD_STATIC, KEYWORD_TYPE_STATIC },
	{ KEYWORD_CONST, KEYWORD_TYPE_CONSTANT },
	{ KEYWORD_EXTERN, KEYWORD_TYPE_EXTERN },
	{ KEYWORD_DLLEXPORT, KEYWORD_TYPE_DLLEXPORT },
	{ KEYWORD_DLLIMPORT, KEYWORD_TYPE_DLLIMPORT },
	{ KEYWORD_PACKED, KEYWORD_TYPE_PACKED },

	{ KEYWORD_IF, KEYWORD_TYPE_IF },
	{ KEYWORD_ELSE, KEYWORD_TYPE_ELSE },
	{ KEYWORD_FOR, KEYWORD_TYPE_FOR },
	{ KEYWORD_WHILE, KEYWORD_TYPE_WHILE },
	{ KEYWORD_RETURN, KEYWORD_TYPE_RETURN },
	{ KEYWORD_BREAK, KEYWORD_TYPE_BREAK },
	{ KEYWORD_CONTINUE, KEYWORD_TYPE_CONTINUE },
	{ KEYWORD_DEFER, KEYWORD_TYPE_DEFER },

	{ KEYWORD_AS, KEYWORD_TYPE_AS },
	{ KEYWORD_SIZEOF, KEYWORD_TYPE_SIZEOF },
	{ KEYWORD_ALLOCA, KEYWORD_TYPE_ALLOCA },
	{ KEYWORD_MALLOC, KEYWORD_TYPE_MALLOC },
	{ KEYWORD_STACKNEW, KEYWORD_TYPE_STACKNEW },
	{ KEYWORD_FREE, KEYWORD_TYPE_FREE },

	{ KEYWORD_TRUE, KEYWORD_TYPE_TRUE },
	{ KEYWORD_FALSE, KEYWORD_TYPE_FALSE },
	{ KEYWORD_NULL, KEYWORD_TYPE_NULL_KEYWORD },

	{ KEYWORD_VOID, KEYWORD_TYPE_VOID },

	{ KEYWORD_INT8, KEYWORD_TYPE_INT8 },
	{ KEYWORD_INT16, KEYWORD_TYPE_INT16 },
	{ KEYWORD_INT32, KEYWORD_TYPE_INT32 },
	{ KEYWORD_INT64, KEYWORD_TYPE_INT64 },
	{ KEYWORD_INT128, KEYWORD_TYPE_INT128 },

	{ KEYWORD_CHAR, KEYWORD_TYPE_INT8 },
	{ KEYWORD_SHORT, KEYWORD_TYPE_INT16 },
	{ KEYWORD_INT, KEYWORD_TYPE_INT32 },
	{ KEYWORD_LONG, KEYWORD_TYPE_INT64 },

	{ KEYWORD_UINT8, KEYWORD_TYPE_UINT8 },
	{ KEYWORD_UINT16, KEYWORD_TYPE_UINT16 },
	{ KEYWORD_UINT32, KEYWORD_TYPE_UINT32 },
	{ KEYWORD_UINT64, KEYWORD_TYPE_UINT64 },
	{ KEYWORD_UINT128, KEYWORD_TYPE_UINT128 },

	{ KEYWORD_BYTE, KEYWORD_TYPE_UINT8 },
	{ KEYWORD_UCHAR, KEYWORD_TYPE_UINT8 },
	{ KEYWORD_USHORT, KEYWORD_TYPE_UINT16 },
	{ KEYWORD_UINT, KEYWORD_TYPE_UINT32 },
	{ KEYWORD_ULONG, KEYWORD_TYPE_UINT64 },

	{ KEYWORD_BOOL, KEYWORD_TYPE_BOOL },

	{ KEYWORD_FLOAT16, KEYWORD_TYPE_FLOAT16 },
	{ KEYWORD_FLOAT32, KEYWORD_TYPE_FLOAT32 },
	{ KEYWORD_FLOAT64, KEYWORD_TYPE_FLOAT64 },
	{ KEYWORD_FLOAT80, KEYWORD_TYPE_FLOAT80 },
	{ KEYWORD_FLOAT128, KEYWORD_TYPE_FLOAT128 },

	{ KEYWORD_HALF, KEYWORD_TYPE_FLOAT16 },
	{ KEYWORD_FLOAT, KEYWORD_TYPE_FLOAT32 },
	{ KEYWORD_DOUBLE, KEYWORD_TYPE_FLOAT64 },
	{ KEYWORD_DECIMAL, KEYWORD_TYPE_FLOAT80 },

	{ KEYWORD_STRING, KEYWORD_TYPE_STRING },
	{ KEYWORD_PTR, KEYWORD_TYPE_PTR },
	{ KEYWORD_ARRAY, KEYWORD_TYPE_ARRAY },
	{ KEYWORD_FUNC_TYPE, KEYWORD_TYPE_FUNC_TYPE },
};


Lexer* CreateLexer(const char* src, const char* filename, SkContext* context)
{
	Lexer* lexer = new Lexer();

	lexer->input = CreateInput(src, filename);
	lexer->filename = filename;

	lexer->context = context;

	return lexer;
}

void DestroyLexer(Lexer* lexer)
{
	DestroyInput(&lexer->input);
}

static bool nextIsWhitespace(Lexer* lexer)
{
	char c = InputPeek(&lexer->input, 0);
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\b';
}

static bool nextIsComment(Lexer* lexer)
{
	return InputPeek(&lexer->input, 0) == '/' && (InputPeek(&lexer->input, 1) == '/' || InputPeek(&lexer->input, 1) == '*');
}

static bool nextIsStringLiteral(Lexer* lexer)
{
	return InputPeek(&lexer->input, 0) == '"';
}

static bool nextIsCharLiteral(Lexer* lexer)
{
	return InputPeek(&lexer->input, 0) == '\'';
}

static bool nextIsNumberLiteral(Lexer* lexer)
{
	char c1 = InputPeek(&lexer->input, 0);
	char c2 = InputPeek(&lexer->input, 1);
	return isdigit(c1) || (c1 == '-' && isdigit(c2));
}

static bool nextIsPunctuation(Lexer* lexer)
{
	char c = InputPeek(&lexer->input, 0);
	return c == '.'
		|| c == ','
		|| c == ':'
		|| c == ';'
		|| c == '#'
		|| c == '('
		|| c == ')'
		|| c == '{'
		|| c == '}'
		|| c == '['
		|| c == ']';
}

static bool nextIsOperator(Lexer* lexer)
{
	char c = InputPeek(&lexer->input, 0);
	return c == '+'
		|| c == '-'
		|| c == '*'
		|| c == '/'
		|| c == '%'
		|| c == '&'
		|| c == '|'
		|| c == '^'
		|| c == '<'
		|| c == '>'
		|| c == '!'
		|| c == '?'
		|| c == '$'
		|| c == '=';
}

static bool nextIsIdentifier(Lexer* lexer)
{
	char c = InputPeek(&lexer->input, 0);
	return isalpha(c) || c == '_';
}

static void skipComment(Lexer* lexer)
{
	InputNext(&lexer->input);
	char c2 = InputNext(&lexer->input);
	if (c2 == '/')
		while (InputNext(&lexer->input) != '\n' && InputHasNext(&lexer->input));
	else if (c2 == '*')
	{
		int level = 1;
		while (level > 0 && InputHasNext(&lexer->input))
		{
			char c = InputNext(&lexer->input);
			if (c == '/' && InputNext(&lexer->input) == '*')
				level++;
			else if (c == '*' && InputNext(&lexer->input) == '/')
				level--;
		}
	}
	else
		assert(false && "SkipComment error");
}

static void skipWhitespace(Lexer* lexer)
{
	while (nextIsWhitespace(lexer) || nextIsComment(lexer))
	{
		if (nextIsComment(lexer))
			skipComment(lexer);
		else
			InputNext(&lexer->input);
	}
}

static Token readStringLiteral(Lexer* lexer)
{
	Token token;
	token.type = TOKEN_TYPE_STRING_LITERAL;
	token.line = lexer->input.state.line;
	token.col = lexer->input.state.col;

	InputNext(&lexer->input);
	token.str = &lexer->input.state.buffer[lexer->input.state.index];
	token.len = 0;

	char c;
	while ((c = InputNext(&lexer->input)) != '"')
	{
		token.len++;
		if (c == '\\')
		{
			InputNext(&lexer->input);
			token.len++;
		}
	}

	return token;
}

static Token readCharLiteral(Lexer* lexer)
{
	Token token;
	token.type = TOKEN_TYPE_CHAR_LITERAL;
	token.line = lexer->input.state.line;
	token.col = lexer->input.state.col;

	InputNext(&lexer->input);
	token.str = &lexer->input.state.buffer[lexer->input.state.index];
	token.len = 0;

	char c;
	while ((c = InputNext(&lexer->input)) != '\'')
	{
		token.len++;
		if (c == '\\')
		{
			InputNext(&lexer->input);
			token.len++;
		}
	}

	return token;
}

static Token readNumberLiteral(Lexer* lexer)
{
	Token token;
	// token.type
	token.line = lexer->input.state.line;
	token.col = lexer->input.state.col;

	token.str = &lexer->input.state.buffer[lexer->input.state.index];
	token.len = 0;

	bool fp = false;

	if (InputPeek(&lexer->input, 0) == '-')
	{
		InputNext(&lexer->input);
		token.len++;
	}

	char c = InputPeek(&lexer->input, 0);
	char c2 = InputPeek(&lexer->input, 1);
	for (;
		isdigit(c) ||
		(!fp && c == '.' && (isalpha(c2) || isdigit(c2))) ||
		c == 'x' ||
		(c >= 'a' && c <= 'f') ||
		(c >= 'A' && c <= 'F') ||
		c == '_';
		)
	{
		InputNext(&lexer->input);
		if (c == '.')
			fp = true;
		token.len++;

		c = InputPeek(&lexer->input, 0);
		c2 = InputPeek(&lexer->input, 1);
	}

	token.type = fp ? TOKEN_TYPE_FLOAT_LITERAL : TOKEN_TYPE_INT_LITERAL;

	return token;
}

static Token readPunctuation(Lexer* lexer)
{
	InputState state = lexer->input.state;

	Token token = {};
	token.type = (TokenType)InputNext(&lexer->input);
	token.line = state.line;
	token.col = state.col;

	token.str = state.ptr; // &lexer->input.state.buffer[lexer->input.state.index];
	token.len = 1;

	return token;
}

static Token readOperator(Lexer* lexer)
{
	InputState state = lexer->input.state;

	Token token = {};
	token.line = lexer->input.state.line;
	token.col = lexer->input.state.col;

	token.str = state.ptr;
	token.len = 1;

	switch (InputNext(&lexer->input))
	{
	case '+': token.type = TOKEN_TYPE_OP_PLUS; break;
	case '-': token.type = TOKEN_TYPE_OP_MINUS; break;
	case '*': token.type = TOKEN_TYPE_OP_ASTERISK; break;
	case '/': token.type = TOKEN_TYPE_OP_SLASH; break;
	case '%': token.type = TOKEN_TYPE_OP_PERCENT; break;
	case '&': token.type = TOKEN_TYPE_OP_AMPERSAND; break;
	case '|': token.type = TOKEN_TYPE_OP_OR; break;
	case '^': token.type = TOKEN_TYPE_OP_CARET; break;
	case '?': token.type = TOKEN_TYPE_OP_QUESTION; break;
	case '!': token.type = TOKEN_TYPE_OP_EXCLAMATION; break;
	case '=': token.type = TOKEN_TYPE_OP_EQUALS; break;
	case '<': token.type = TOKEN_TYPE_OP_LESS_THAN; break;
	case '>': token.type = TOKEN_TYPE_OP_GREATER_THAN; break;
	default:
		assert(false && "ReadOperator");
		break;
	}

	return token;
}

static KeywordType getKeywordType(const char* str, int len)
{
	for (const auto& pair : keywords) {
		if (pair.first.length() == len && strncmp(pair.first.c_str(), str, len) == 0)
			return pair.second;
	}
	return KEYWORD_TYPE_NULL;
}

static Token readIdentifier(Lexer* lexer)
{
	Token token;
	token.type = TOKEN_TYPE_IDENTIFIER;
	token.line = lexer->input.state.line;
	token.col = lexer->input.state.col;

	token.str = &lexer->input.state.buffer[lexer->input.state.index];
	token.len = 0;

	char c = 0;
	for (c = InputPeek(&lexer->input, 0); (isalpha(c) || isdigit(c) || c == '_'); c = InputPeek(&lexer->input, 0))
	{
		InputNext(&lexer->input);
		token.len++;
	}

	if (KeywordType keywordType = getKeywordType(token.str, token.len))
		token.keywordType = keywordType;

	return token;
}

Token LexerNext(Lexer* lexer)
{
	skipWhitespace(lexer);

	if (InputHasNext(&lexer->input))
	{
		if (nextIsStringLiteral(lexer))
			return readStringLiteral(lexer);
		if (nextIsCharLiteral(lexer))
			return readCharLiteral(lexer);
		if (nextIsNumberLiteral(lexer))
			return readNumberLiteral(lexer);
		if (nextIsPunctuation(lexer))
			return readPunctuation(lexer);
		if (nextIsOperator(lexer))
			return readOperator(lexer);
		if (nextIsIdentifier(lexer))
			return readIdentifier(lexer);

		SnekError(lexer->context, lexer->input.state, ERROR_CODE_UNDEFINED_CHARACTER, "Undefined character '%c' (%d)", InputNext(&lexer->input));
	}

	return NULL_TOKEN;
}

Token LexerPeek(Lexer* lexer, int offset)
{
	skipWhitespace(lexer);
	InputState state = lexer->input.state;

	for (int i = 0; i < offset; i++)
		LexerNext(lexer);

	Token token = LexerNext(lexer);
	lexer->input.state = state;

	return token;
}

bool LexerHasNext(Lexer* lexer)
{
	return LexerPeek(lexer).type != TOKEN_TYPE_NULL;
}

bool LexerNextIsWhitespace(Lexer* lexer)
{
	return nextIsWhitespace(lexer);
}

char* GetTokenString(Token token)
{
	char* str = new char[token.len + 1];
	memcpy(str, token.str, token.len);
	str[token.len] = 0;
	return str;
}
