#pragma once

#include <stdio.h>


#if defined(_DEBUG)
#define SnekAssert(condition) { if (!(condition)) { fprintf(stderr, "Assertion %s failed at %s line %d", #condition, __FILE__, __LINE__); __debugbreak(); } }
#define SnekAssertMsg(condition, message) { if (!(condition)) { fprintf(stderr, "Assertion %s failed at %s line %d: %s", #condition, __FILE__, __LINE__, message); __debugbreak(); } }
#else
#define SnekAssert(condition)
#define SnekAssertMsg(condition, message)
#endif

#define SnekTrace(context, msg, ...) GetMsgCallback(context)(MESSAGE_TYPE_INFO, NULL, 0, 0, 0, msg, ##__VA_ARGS__)
#define SnekWarn(context, location, code, msg, ...) GetMsgCallback(context)(MESSAGE_TYPE_WARNING, (location).filename, (location).line, (location).col, code, msg, ##__VA_ARGS__)
#define SnekError(context, location, code, msg, ...) GetMsgCallback(context)(MESSAGE_TYPE_ERROR, (location).filename, (location).line, (location).col, code, msg, ##__VA_ARGS__)
#define SnekFatal(context, code, msg, ...) GetMsgCallback(context)(MESSAGE_TYPE_FATAL_ERROR, NULL, 0, 0, code, msg, ##__VA_ARGS__)


enum MessageType
{
	MESSAGE_TYPE_NULL = 0,

	MESSAGE_TYPE_INFO,
	MESSAGE_TYPE_WARNING,
	MESSAGE_TYPE_ERROR,
	MESSAGE_TYPE_FATAL_ERROR,

	MESSAGE_TYPE_MAX,
};

typedef void(*MessageCallback_t)(MessageType msgType, const char* filename, int line, int col, int errCode, const char* msg, ...);

enum MessageCode
{
	ERROR_CODE_FIRST = 1111,

	ERROR_CODE_FILE_NOT_FOUND,
	ERROR_CODE_FOLDER_NOT_FOUND,
	ERROR_CODE_CMD_ARG_SYNTAX,

	ERROR_CODE_BACKEND_ERROR,
	ERROR_CODE_ENTRY_POINT_DECLARATION,

	ERROR_CODE_GLOBAL_TYPE_MISMATCH,
	ERROR_CODE_GLOBAL_INITIALIZER_NONCONSTANT,
	ERROR_CODE_GLOBAL_EXTERN_INITIALIZER,

	ERROR_CODE_UNDEFINED_CHARACTER,
	ERROR_CODE_UNEXPECTED_TOKEN,
	ERROR_CODE_INVALID_LITERAL,
	ERROR_CODE_UNKNOWN_MODULE,

	ERROR_CODE_EXPRESSION_EXPECTED,
	ERROR_CODE_FOR_LOOP_SYNTAX,
	ERROR_CODE_STRUCT_SYNTAX,
	ERROR_CODE_CLASS_SYNTAX,
	ERROR_CODE_FUNCTION_SYNTAX,
	ERROR_CODE_MODULE_SYNTAX,
	ERROR_CODE_IMPORT_SYNTAX,

	ERROR_CODE_INVALID_BREAK,
	ERROR_CODE_INVALID_CONTINUE,

	ERROR_CODE_ARRAY_LENGTH_WRONG_TYPE,
	ERROR_CODE_NON_INVOKABLE_EXPRESSION,
	ERROR_CODE_MALLOC_INVALID_TYPE,
	ERROR_CODE_DEREFERENCE_INVALID_TYPE,
	ERROR_CODE_BIN_OP_INVALID_TYPE,
	ERROR_CODE_FUNCTION_CALL_ARGUMENT_MISMATCH,
	ERROR_CODE_UNRESOLVED_IDENTIFIER,
	ERROR_CODE_UNRESOLVED_MEMBER,
	ERROR_CODE_UNDEFINED_TYPE,
	ERROR_CODE_INVALID_CAST,
	ERROR_CODE_RETURN_WRONG_TYPE,
	ERROR_CODE_TERNARY_OPERATOR_TYPE_MISMATCH,
	ERROR_CODE_NON_VISIBLE_DECLARATION,
	ERROR_CODE_CONSTANT_INITIALIZER_NON_CONSTANT,


	// WARNINGS

	ERROR_CODE_VARIABLE_SHADOWING,
};


MessageCallback_t GetMsgCallback(struct SkContext* context);
