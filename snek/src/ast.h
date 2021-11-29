#pragma once

#include "list.h"
#include "input.h"
#include "type.h"

#include <new>


#define FUNC_MAX_PARAMS 32


typedef void* ValueHandle;
typedef void* ControlFlowHandle;
typedef void* TypeHandle;

struct SourceFile;
struct AstModule;
struct AstFile;
struct AstExpression;

struct AstElement
{
	AstFile* module;
	InputState inputState;
};


enum AstTypeKind : uint8_t
{
	TYPE_KIND_NULL = 0,

	TYPE_KIND_VOID,
	TYPE_KIND_INTEGER,
	TYPE_KIND_FP,
	TYPE_KIND_BOOL,
	TYPE_KIND_NAMED_TYPE,
	TYPE_KIND_STRUCT,
	TYPE_KIND_CLASS,
	TYPE_KIND_ALIAS,
	TYPE_KIND_POINTER,
	TYPE_KIND_FUNCTION,
	TYPE_KIND_ARRAY,
	TYPE_KIND_STRING,
};

struct AstType : AstElement
{
	AstTypeKind typeKind;

	TypeID typeID;
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

struct AstNamedType : AstType
{
	char* name;

	struct AstStruct* structDecl;
	struct AstClass* classDecl;
	struct AstTypedef* typedefDecl;
	struct AstEnum* enumDecl;
};

struct AstPointerType : AstType
{
	AstType* elementType;
};

struct AstFunctionType : AstType
{
	AstType* returnType;
	List<AstType*> paramTypes;
	bool varArgs;
};

struct AstArrayType : AstType
{
	AstType* elementType;
	AstExpression* length;
};

struct AstStringType : AstType
{
};

struct AstTypeStorage
{
	union
	{
		AstVoidType voidType;
		AstIntegerType integerType;
		AstFPType fpType;
		AstBoolType boolType;
		AstNamedType classType;
		AstPointerType pointerType;
		AstFunctionType functionType;
		AstArrayType arrayType;
		AstStringType stringType;
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
	EXPR_KIND_STRING_LITERAL,
	EXPR_KIND_STRUCT_LITERAL,
	EXPR_KIND_IDENTIFIER,
	EXPR_KIND_COMPOUND,

	EXPR_KIND_FUNC_CALL,
	EXPR_KIND_SUBSCRIPT_OPERATOR,
	EXPR_KIND_DOT_OPERATOR,
	EXPR_KIND_CAST,
	EXPR_KIND_SIZEOF,
	EXPR_KIND_MALLOC,

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

enum AstVisibility
{
	VISIBILITY_NULL = 0,
	VISIBILITY_PRIVATE,
	VISIBILITY_PUBLIC,
};

struct AstVariable : AstElement
{
	char* name;
	char* mangledName;
	bool isConstant;
	AstVisibility visibility;
	AstExpression* value;

	AstFile* module;
	struct AstGlobal* globalDecl;

	TypeID type;

	ValueHandle allocHandle;
};

struct AstExpression : AstElement
{
	AstExpressionKind exprKind;

	TypeID type;
	bool lvalue;
};

struct AstIntegerLiteral : AstExpression
{
	int64_t value;
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

struct AstStringLiteral : AstExpression
{
	char* value;
	int length;
};

struct AstStructLiteral : AstExpression
{
	AstType* structType;
	List<AstExpression*> values;
};

struct AstFunction;
struct AstEnumValue;

struct AstIdentifier : AstExpression
{
	char* name;

	AstVariable* variable;
	AstFunction* function;
	AstExpression* exprdefValue;
	AstEnumValue* enumValue;
};

struct AstCompoundExpression : AstExpression
{
	AstExpression* value;
};

struct AstFuncCall : AstExpression
{
	AstExpression* calleeExpr;
	List<AstExpression*> arguments;

	AstFunction* function;
	bool isMethodCall;
	AstExpression* methodInstance;
};

struct AstSubscriptOperator : AstExpression
{
	AstExpression* operand;
	List<AstExpression*> arguments;
};

struct AstStructField;
struct AstClassField;

struct AstDotOperator : AstExpression
{
	AstExpression* operand;
	char* name;

	AstModule* ns;
	AstFunction* namespacedFunction;
	AstVariable* namespacedVariable;

	union
	{
		struct { // structs
			AstStructField* structField;
			AstFunction* classMethod;
			ValueHandle methodInstance;
		};
		struct { // classes
			AstClassField* classField;
		};
		struct { // arrays
			int arrayField;
		};
		struct { // strings
			int stringField;
		};
	};
};

struct AstCast : AstExpression
{
	AstExpression* value;
	AstType* castType;
};

struct AstSizeof : AstExpression
{
	AstType* sizedType;
};

struct AstMalloc : AstExpression
{
	AstType* mallocType;
	AstExpression* count;

	bool hasArguments;
	List<AstExpression*> arguments;
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
		AstStringLiteral stringLiteral;
		AstStructLiteral structLiteral;
		AstIdentifier identifier;
		AstCompoundExpression compound;

		AstFuncCall funcCall;
		AstSubscriptOperator subscriptOperator;
		AstDotOperator dotOperator;
		AstCast cast;
		AstSizeof sizeOf;
		AstMalloc malloc;

		AstUnaryOperator unaryOperator;
		AstBinaryOperator binaryOperator;
		AstTernaryOperator ternaryOperator;
	};
};


enum AstStatementKind : uint8_t
{
	STATEMENT_KIND_NULL = 0,

	STATEMENT_KIND_NO_OP,
	STATEMENT_KIND_COMPOUND,
	STATEMENT_KIND_EXPR,
	STATEMENT_KIND_VAR_DECL,
	STATEMENT_KIND_IF,
	STATEMENT_KIND_WHILE,
	STATEMENT_KIND_FOR,
	STATEMENT_KIND_BREAK,
	STATEMENT_KIND_CONTINUE,
	STATEMENT_KIND_RETURN,
	STATEMENT_KIND_FREE,
};

struct AstStatement : AstElement
{
	AstStatementKind statementKind;
};

struct AstNoOpStatement : AstStatement
{
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
	bool isConstant;
	List<AstVarDeclarator> declarators;
};

struct AstIfStatement : AstStatement
{
	AstExpression* condition;
	AstStatement* thenStatement, * elseStatement;
};

struct AstWhileLoop : AstStatement
{
	AstExpression* condition;
	AstStatement* body;

	ControlFlowHandle breakHandle;
	ControlFlowHandle continueHandle;
};

struct AstForLoop : AstStatement
{
	char* iteratorName;
	AstExpression* startValue, * endValue, * deltaValue;
	AstStatement* body;
	int direction;

	AstVariable* iterator;

	ControlFlowHandle breakHandle;
	ControlFlowHandle continueHandle;
};

struct AstBreak : AstStatement
{
	AstStatement* branchDst;
};

struct AstContinue : AstStatement
{
	AstStatement* branchDst;
};

struct AstReturn : AstStatement
{
	AstExpression* value;
};

struct AstFree : AstStatement
{
	List<AstExpression*> values;
};

struct AstStatementStorage
{
	union
	{
		AstNoOpStatement noOpStatement;
		AstExprStatement exprStatement;
		AstCompoundStatement compoundStatement;
		AstVarDeclStatement varDeclStatement;
		AstIfStatement ifStatement;
		AstWhileLoop whileLoop;
		AstForLoop forLoop;
		AstBreak breakStatement;
		AstContinue continueStatement;
		AstReturn returnStatement;
		AstFree freeStatement;
	};
};


enum AstDeclarationKind : uint8_t
{
	DECL_KIND_NULL = 0,

	DECL_KIND_FUNC,
	DECL_KIND_STRUCT,
	DECL_KIND_CLASS,
	DECL_KIND_CLASS_METHOD,
	DECL_KIND_CLASS_CONSTRUCTOR,
	DECL_KIND_TYPEDEF,
	DECL_KIND_ENUM,
	DECL_KIND_EXPRDEF,
	DECL_KIND_GLOBAL,
	DECL_KIND_MODULE,
	DECL_KIND_NAMESPACE,
	DECL_KIND_IMPORT,
};

enum AstDeclFlagBits
{
	DECL_FLAG_NONE = 0,
	DECL_FLAG_CONSTANT = 1 << 0,
	DECL_FLAG_EXTERN = 1 << 1,
	DECL_FLAG_LINKAGE_DLLEXPORT = 1 << 2,
	DECL_FLAG_LINKAGE_DLLIMPORT = 1 << 3,
	DECL_FLAG_VISIBILITY_PUBLIC = 1 << 4,
	DECL_FLAG_VISIBILITY_PRIVATE = 1 << 5,
	DECL_FLAG_PACKED = 1 << 6,
};

typedef uint32_t AstDeclFlags;

struct AstDeclaration : AstElement
{
	AstDeclarationKind declKind;
	AstDeclFlags flags;
	AstVisibility visibility;
};

struct AstFunction : AstDeclaration
{
	InputState endInputState;

	char* name;
	AstType* returnType;
	List<AstType*> paramTypes;
	List<char*> paramNames;
	bool varArgs;
	AstStatement* body;

	TypeID instanceType; // For class methods/constructors
	//int functionID;
	char* mangledName;
	TypeID type;

	List<AstVariable*> paramVariables;
	AstVariable* instanceVariable;

	ValueHandle valueHandle;
};

struct AstStructField : AstElement
{
	AstType* type;
	char* name;
	int index;
};

struct AstStruct : AstDeclaration
{
	char* name;
	bool hasBody;
	List<AstStructField> fields;

	char* mangledName;
	TypeID type;

	TypeHandle typeHandle;
};

struct AstClassField : AstElement
{
	AstType* type;
	char* name;
	int index;
};

struct AstClass : AstDeclaration
{
	char* name;
	List<AstClassField> fields;
	List<AstFunction*> methods;
	AstFunction* constructor;

	char* mangledName;
	TypeID type;

	TypeHandle typeHandle;
};

struct AstTypedef : AstDeclaration
{
	char* name;
	AstType* alias;

	TypeID type;
};

struct AstEnumValue
{
	char* name;
	AstExpression* value;
	struct AstEnum* enumDecl;

	ValueHandle valueHandle;
};

struct AstEnum : AstDeclaration
{
	char* name;
	AstType* alias;
	List<AstEnumValue> values;

	TypeID type;
};

struct AstExprdef : AstDeclaration
{
	char* name;
	AstExpression* alias;
};

struct AstGlobal : AstDeclaration
{
	AstType* type;
	char* name;
	AstExpression* value;

	AstVariable* variable;
	ValueHandle constValue;
};

struct AstModuleDecl : AstDeclaration
{
	List<char*> namespaces;

	AstModule* ns;
};

struct AstNamespaceDecl : AstDeclaration
{
	char* name;
};

struct AstImport : AstDeclaration
{
	List<List<char*>> imports;

	//List<AstModule*> modules;
};

struct AstDeclarationStorage
{
	union
	{
		AstFunction funcDecl;
		AstStruct structDecl;
		AstClass classDecl;
		AstTypedef typedefDecl;
		AstEnum enumDecl;
		AstExprdef exprdefDecl;
		AstGlobal globalDecl;
		AstModuleDecl moduleDecl;
		AstNamespaceDecl namespaceDecl;
		AstImport importDecl;
	};
};


const int ELEMENT_BLOCK_NUM_ELEMENTS = 256;
const int ELEMENT_BLOCK_SIZE = (ELEMENT_BLOCK_NUM_ELEMENTS * 2 * sizeof(AstElement));

struct AstElementBlock
{
	char elements[ELEMENT_BLOCK_SIZE];
};

struct AstFile
{
	char* name;
	int moduleID;
	SourceFile* sourceFile;

	List<AstElementBlock*> blocks;
	int index;

	AstModule* module;
	char* nameSpace;

	AstModuleDecl* moduleDecl;
	AstNamespaceDecl* namespaceDecl;

	List<AstFunction*> functions;
	List<AstStruct*> structs;
	List<AstClass*> classes;
	List<AstTypedef*> typedefs;
	List<AstEnum*> enums;
	List<AstExprdef*> exprdefs;
	List<AstGlobal*> globals;
	List<AstImport*> imports;

	List<AstModule*> dependencies;
};

struct AstModule
{
	char* name;
	AstModule* parent;
	List<AstModule*> children;
	List<AstFile*> files;
};


AstFile* CreateAst(char* name, int moduleID, SourceFile* sourceFile);

AstType* CreateAstType(AstFile* module, const InputState& inputState, AstTypeKind typeKind);
AstExpression* CreateAstExpression(AstFile* module, const InputState& inputState, AstExpressionKind exprKind);
AstStatement* CreateAstStatement(AstFile* module, const InputState& inputState, AstStatementKind statementKind);
AstDeclaration* CreateAstDeclaration(AstFile* module, const InputState& inputState, AstDeclarationKind declKind);
AstVariable* CreateAstVariable(AstFile* file, const InputState& inputState);

template<typename T>
T* CreateAstType(AstFile* module, const InputState& inputState, AstTypeKind typeKind) { return (T*)CreateAstType(module, inputState, typeKind); }

template<typename T>
T* CreateAstExpression(AstFile* module, const InputState& inputState, AstExpressionKind exprKind) { return (T*)CreateAstExpression(module, inputState, exprKind); }

template<typename T>
T* CreateAstStatement(AstFile* module, const InputState& inputState, AstStatementKind statementKind) { return (T*)CreateAstStatement(module, inputState, statementKind); }

template<typename T>
T* CreateAstDeclaration(AstFile* module, const InputState& inputState, AstDeclarationKind declKind) { return (T*)CreateAstDeclaration(module, inputState, declKind); }

AstType* CopyType(AstType* type, AstFile* module);
AstExpression* CopyExpression(AstExpression* expression, AstFile* module);

bool IsLiteral(AstExpression* expression);
bool IsConstant(AstExpression* expr);
