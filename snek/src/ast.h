#pragma once

#include "list.h"
#include "input.h"

#include <new>


#define FUNC_MAX_PARAMS 32


struct AstModule;

struct AstElement
{
	AstModule* module;
	InputState inputState;
};


enum AstTypeKind : uint8_t
{
	TYPE_KIND_NULL = 0,

	TYPE_KIND_VOID,
	TYPE_KIND_INTEGER,
	TYPE_KIND_FP,
	TYPE_KIND_BOOL,
	TYPE_KIND_POINTER,
};

struct AstType : AstElement
{
	AstTypeKind typeKind;
};

struct AstVoidType : AstType
{
};

struct AstIntegerType : AstType
{
	int bitWidth;
	bool isSigned;
};

struct AstFPType : AstType
{
	int bitWidth;
};

struct AstBoolType : AstType
{
};

struct AstPointerType : AstType
{
	AstType* elementType;
};

struct AstTypeStorage
{
	union
	{
		AstVoidType voidType;
		AstIntegerType integerType;
		AstFPType fpType;
		AstBoolType boolType;
		AstPointerType pointerType;
	};
};


enum AstExpressionKind : uint8_t
{
	EXPR_KIND_NULL = 0,

	EXPR_KIND_INTEGER_LITERAL,
	EXPR_KIND_FP_LITERAL,
	EXPR_KIND_BOOL_LITERAL,
	EXPR_KIND_CHARACTER_LITERAL,
	EXPR_KIND_NULL_LITERAL,
	EXPR_KIND_IDENTIFIER,
	EXPR_KIND_COMPOUND,

	EXPR_KIND_FUNC_CALL,
	EXPR_KIND_SUBSCRIPT_OPERATOR,
	EXPR_KIND_DOT_OPERATOR,
	EXPR_KIND_CAST,

	EXPR_KIND_UNARY_OPERATOR,
	EXPR_KIND_BINARY_OPERATOR,
	EXPR_KIND_TERNARY_OPERATOR,
};

enum AstUnaryOperatorType : uint8_t
{
	UNARY_OPERATOR_NULL = 0,

	UNARY_OPERATOR_NOT,
	UNARY_OPERATOR_NEGATE,
	UNARY_OPERATOR_REFERENCE,
	UNARY_OPERATOR_DEREFERENCE,

	UNARY_OPERATOR_INCREMENT,
	UNARY_OPERATOR_DECREMENT,
};

enum AstBinaryOperatorType : uint8_t
{
	BINARY_OPERATOR_NULL = 0,

	BINARY_OPERATOR_ADD,
	BINARY_OPERATOR_SUB,
	BINARY_OPERATOR_MUL,
	BINARY_OPERATOR_DIV,
	BINARY_OPERATOR_MOD,

	BINARY_OPERATOR_EQ,
	BINARY_OPERATOR_NE,
	BINARY_OPERATOR_LT,
	BINARY_OPERATOR_GT,
	BINARY_OPERATOR_LE,
	BINARY_OPERATOR_GE,
	BINARY_OPERATOR_AND,
	BINARY_OPERATOR_OR,
	BINARY_OPERATOR_BITWISE_AND,
	BINARY_OPERATOR_BITWISE_OR,
	BINARY_OPERATOR_BITWISE_XOR,
	BINARY_OPERATOR_BITSHIFT_LEFT,
	BINARY_OPERATOR_BITSHIFT_RIGHT,

	BINARY_OPERATOR_ASSIGN,
	BINARY_OPERATOR_ADD_ASSIGN,
	BINARY_OPERATOR_SUB_ASSIGN,
	BINARY_OPERATOR_MUL_ASSIGN,
	BINARY_OPERATOR_DIV_ASSIGN,
	BINARY_OPERATOR_MOD_ASSIGN,
	BINARY_OPERATOR_BITSHIFT_LEFT_ASSIGN,
	BINARY_OPERATOR_BITSHIFT_RIGHT_ASSIGN,
	BINARY_OPERATOR_BITWISE_AND_ASSIGN,
	BINARY_OPERATOR_BITWISE_OR_ASSIGN,
	BINARY_OPERATOR_BITWISE_XOR_ASSIGN,
	BINARY_OPERATOR_REF_ASSIGN,

	BINARY_OPERATOR_TERNARY,
};

struct AstVariable
{
	char* name;
	char* mangledName;
};

struct AstExpression : AstElement
{
	AstExpressionKind exprKind;
	bool resolved;

	AstType* type;
};

struct AstIntegerLiteral : AstExpression
{
	unsigned long long value;
};

struct AstFPLiteral : AstExpression
{
	double value;
};

struct AstBoolLiteral : AstExpression
{
	bool value;
};

struct AstCharacterLiteral : AstExpression
{
	uint32_t value;
};

struct AstNullLiteral : AstExpression
{
};

struct AstIdentifier : AstExpression
{
	struct AstFunction;

	char* name;

	AstVariable* variable;
	AstFunction* function;
};

struct AstCompoundExpression : AstExpression
{
	AstExpression* value;
};

struct AstFuncCall : AstExpression
{
	struct AstFunctionDecl;

	AstExpression* calleeExpr;
	List<AstExpression*> arguments;

	AstFunctionDecl* func = NULL;
};

struct AstSubscriptOperator : AstExpression
{
	AstExpression* operand;
	List<AstExpression*> arguments;
};

struct AstDotOperator : AstExpression
{
	AstExpression* operand;
	char* name;
};

struct AstCast : AstExpression
{
	AstExpression* value;
	AstType* castType;
};

struct AstUnaryOperator : AstExpression
{
	AstExpression* operand;
	AstUnaryOperatorType operatorType;
	bool position;
};

struct AstBinaryOperator : AstExpression
{
	AstExpression* left, * right;
	AstBinaryOperatorType operatorType;
};

struct AstTernaryOperator : AstExpression
{
	AstExpression* condition, * thenValue, * elseValue;
};

struct AstExpressionStorage
{
	union
	{
		AstIntegerLiteral integerLiteral;
		AstFPLiteral fpLiteral;
		AstBoolLiteral boolLiteral;
		AstCharacterLiteral characterLiteral;
		AstBinaryOperator binaryOperation;
		AstNullLiteral nullLiteral;
		AstIdentifier identifier;
		AstCompoundExpression compound;

		AstFuncCall funcCall;
		AstSubscriptOperator subscriptOperator;
		AstDotOperator dotOperator;
		AstCast cast;

		AstUnaryOperator unaryOperator;
		AstBinaryOperator binaryOperator;
		AstTernaryOperator ternaryOperator;
	};
};


enum AstStatementKind : uint8_t
{
	STATEMENT_KIND_NULL = 0,

	STATEMENT_KIND_COMPOUND,
	STATEMENT_KIND_EXPR,
	STATEMENT_KIND_VAR_DECL,
};

struct AstStatement : AstElement
{
	AstStatementKind statementKind;
};

struct AstExprStatement : AstStatement
{
	AstExpression* expr;
};

struct AstCompoundStatement : AstStatement
{
	List<AstStatement*> statements;
};

struct AstVarDeclarator
{
	char* name;
	AstExpression* value;

	AstVariable* variable;
};

struct AstVarDeclStatement : AstStatement
{
	AstType* type;
	List<AstVarDeclarator> declarators;
};

struct AstStatementStorage
{
	union
	{
		AstExprStatement exprStatement;
		AstCompoundStatement compoundStatement;
		AstVarDeclStatement varDeclStatement;
	};
};


enum AstDeclarationKind : uint8_t
{
	DECL_KIND_NULL = 0,

	DECL_KIND_FUNC,
};

struct AstDeclaration : AstElement
{
	AstDeclarationKind declKind;
};

struct AstFunction : AstDeclaration
{
	AstType* returnType;
	char* name;
	List<AstType*> paramTypes;
	List<char*> paramNames;
	AstStatement* body;

	char* mangledName;
};

struct AstDeclarationStorage
{
	union
	{
		AstFunction funcDecl;
	};
};


const int ELEMENT_BLOCK_NUM_ELEMENTS = 32;
const int ELEMENT_BLOCK_SIZE = (ELEMENT_BLOCK_NUM_ELEMENTS * 2 * sizeof(AstElement));

struct AstElementBlock
{
	char elements[ELEMENT_BLOCK_SIZE];
};

struct AstModule
{
	char* name;
	int moduleID;

	List<AstElementBlock*> blocks;
	int index;

	List<AstDeclaration*> declarations;
};


AstModule* CreateAst(char* name, int moduleID);

AstType* CreateAstType(AstModule* module, const InputState& inputState, AstTypeKind typeKind);
AstExpression* CreateAstExpression(AstModule* module, const InputState& inputState, AstExpressionKind exprKind);
AstStatement* CreateAstStatement(AstModule* module, const InputState& inputState, AstStatementKind statementKind);
AstDeclaration* CreateAstDeclaration(AstModule* module, const InputState& inputState, AstDeclarationKind declKind);

template<typename T>
T* CreateAstType(AstModule* module, const InputState& inputState, AstTypeKind typeKind) { return (T*)CreateAstType(module, inputState, typeKind); }

template<typename T>
T* CreateAstExpression(AstModule* module, const InputState& inputState, AstExpressionKind exprKind) { return (T*)CreateAstExpression(module, inputState, exprKind); }

template<typename T>
T* CreateAstStatement(AstModule* module, const InputState& inputState, AstStatementKind statementKind) { return (T*)CreateAstStatement(module, inputState, statementKind); }

template<typename T>
T* CreateAstDeclaration(AstModule* module, const InputState& inputState, AstDeclarationKind declKind) { return (T*)CreateAstDeclaration(module, inputState, declKind); }

/*
template<typename T, class ...Args>
T* CreateAstElement(AstModule* module, const InputState& inputState, Args... args)
{
	AstElement* element = CreateAstElement(module, sizeof(T));
	new (element) T(args...);

	element->module = module;
	element->inputState = inputState;
	return (T*)element;
}
*/
