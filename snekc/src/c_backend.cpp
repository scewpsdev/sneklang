#include "c_backend.h"

#include "ast.h"
#include "stringbuffer.h"
#include "log.h"
#include "snek.h"
#include "file.h"

#include <libtcc/libtcc.h>


struct CModule
{
	AstFile* ast;
	AstFunction* currentFunction;

	StringBuffer globalBuffer;
	List<StringBuffer> functionBuffers;

	StringBuffer* selectedBuffer;

	int indentation;

	List<char*> queuedStatements;
};

struct CBackend
{
	SkContext* context;

	CModule* modules;
	int numModules;

	TCCState* tcc;
};


CBackend* CreateCBackend(SkContext* context)
{
	CBackend* cb = new CBackend();

	cb->context = context;

	cb->tcc = tcc_new();

	return cb;
}

void DestroyCBackend(CBackend* cb)
{
	tcc_delete(cb->tcc);

	delete cb;
}

static StringBuffer& GetGlobalBuffer(CBackend* cb, CModule* module)
{
	return module->globalBuffer;
}

static StringBuffer& GetFunctionBuffer(CBackend* cb, CModule* module, int functionID)
{
	return module->functionBuffers[functionID];
}

static void SelectBuffer(CBackend* cb, CModule* module, StringBuffer& buffer)
{
	module->selectedBuffer = &buffer;
}

static StringBuffer& GetSelectedBuffer(CBackend* cb, CModule* module)
{
	return *module->selectedBuffer;
}

static void PushIndentation(CBackend* cb, CModule* module)
{
	module->indentation++;
}

static void PopIndentation(CBackend* cb, CModule* module)
{
	module->indentation--;
}

static void NewLine(CBackend* cb, CModule* module)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "\n";
	for (int i = 0; i < module->indentation; i++)
		buffer << '\t';
}

static void GenTypeID(CBackend* cb, CModule* module, TypeID type, AstType* ast = NULL);
static void GenType(CBackend* cb, CModule* module, AstType* type);
static void GenExpression(CBackend* cb, CModule* module, AstExpression* expression);

static void GenVoidType(CBackend* cb, CModule* module)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "void";
}

static void GenIntegerType(CBackend* cb, CModule* module, TypeID type)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	if (!type->integerType.isSigned)
		buffer << "unsigned ";

	switch (type->integerType.bitWidth)
	{
	case 8: buffer << "char"; break;
	case 16: buffer << "short"; break;
	case 32: buffer << "int"; break;
	case 64: buffer << "long long"; break;
	default:
		SnekAssert(false);
		break;
	}
}

static void GenFPType(CBackend* cb, CModule* module, TypeID type)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	switch (type->fpType.precision)
	{
	case FP_PRECISION_SINGLE: buffer << "float"; break;
	case FP_PRECISION_DOUBLE: buffer << "double"; break;
	default:
		SnekAssert(false);
		break;
	}
}

static void GenBoolType(CBackend* cb, CModule* module)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "_Bool";
}

static void GenStructHeader(CBackend* cb, CModule* module, AstStruct* decl);

static void GenStructType(CBackend* cb, CModule* module, TypeID type, AstNamedType* ast)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	// If the called function is defined below the current function or from a different module, generate a declaration for it
	if (ast && ast->module != ast->structDecl->module)
	{
		GenStructHeader(cb, module, ast->structDecl);
	}

	buffer << "struct " << type->structType.name;
}

static void GenClassHeader(CBackend* cb, CModule* module, AstClass* decl);

static void GenClassType(CBackend* cb, CModule* module, TypeID type, AstNamedType* ast)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	// If the called function is defined below the current function or from a different module, generate a declaration for it
	if (ast && ast->module != ast->classDecl->module)
	{
		GenClassHeader(cb, module, ast->classDecl);
	}

	buffer << "struct " << type->classType.name << '*';
}

static void GenPointerType(CBackend* cb, CModule* module, TypeID type, AstPointerType* ast)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	GenTypeID(cb, module, type->pointerType.elementType, ast ? ast->elementType : NULL);
	buffer << '*';
}

static void GenStringType(CBackend* cb, CModule* module, TypeID type)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "string";
}

static void GenArrayType(CBackend* cb, CModule* module, TypeID type, AstArrayType* ast)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "struct { int length; ";
	GenTypeID(cb, module, type->arrayType.elementType, ast ? ast->elementType : NULL);
	buffer << " buffer";
	if (type->arrayType.length)
	{
		buffer << '[';
		//GenExpression(cb, module, type->arrayType.length);
		buffer << ']';
	}
	buffer << "; }";
}

static void GenTypeID(CBackend* cb, CModule* module, TypeID type, AstType* ast)
{
	switch (type->typeKind)
	{
	case TYPE_KIND_VOID:
		GenVoidType(cb, module);
		break;
	case TYPE_KIND_INTEGER:
		GenIntegerType(cb, module, type);
		break;
	case TYPE_KIND_FP:
		GenFPType(cb, module, type);
		break;
	case TYPE_KIND_BOOL:
		GenBoolType(cb, module);
		break;
	case TYPE_KIND_STRUCT:
		GenStructType(cb, module, type, (AstNamedType*)ast);
		break;
	case TYPE_KIND_CLASS:
		GenClassType(cb, module, type, (AstNamedType*)ast);
		break;
	case TYPE_KIND_POINTER:
		GenPointerType(cb, module, type, (AstPointerType*)ast);
		break;

	case TYPE_KIND_STRING:
		GenStringType(cb, module, type);
		break;

	case TYPE_KIND_ARRAY:
		GenArrayType(cb, module, type, (AstArrayType*)ast);
		break;

	default:
		SnekAssert(false);
		break;
	}
}

static void GenType(CBackend* cb, CModule* module, AstType* type)
{
	GenTypeID(cb, module, type->typeID, type);
}

static void GenExpression(CBackend* cb, CModule* module, AstExpression* expression);

static void GenIntegerLiteral(CBackend* cb, CModule* module, AstIntegerLiteral* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << (unsigned long)expression->value;
}

static void GenFPLiteral(CBackend* cb, CModule* module, AstFPLiteral* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	StringBufferAppend(buffer, expression->value);
}

static void GenBoolLiteral(CBackend* cb, CModule* module, AstBoolLiteral* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << expression->value;
}

static void GenCharacterLiteral(CBackend* cb, CModule* module, AstCharacterLiteral* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << '\'' << (char)expression->value << '\'';
}

static void GenNullLiteral(CBackend* cb, CModule* module, AstNullLiteral* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "NULL";
}

static void GenStringLiteral(CBackend* cb, CModule* module, AstStringLiteral* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "__new_string(\"";
	for (int i = 0; i < expression->length; i++)
	{
		char c = expression->value[i];
		switch (c)
		{
		case '\n': buffer << "\\n"; break;
		case '\r': buffer << "\\r"; break;
		case '\t': buffer << "\\t"; break;
		case '\\': buffer << "\\\\"; break;
		case '\0': buffer << "\\0"; break;
		default: buffer << c; break;
		}
	}
	buffer << "\")";
}

static void GenIdentifier(CBackend* cb, CModule* module, AstIdentifier* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	if (expression->variable)
	{
		buffer << expression->variable->mangledName;
	}
	else if (expression->function)
	{
		buffer << expression->function->mangledName;
	}
	else
	{
		SnekAssert(false);
	}
}

static void GenCompoundExpression(CBackend* cb, CModule* module, AstCompoundExpression* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << '(';
	GenExpression(cb, module, expression->value);
	buffer << ')';
}

static void GenFunctionHeader(CBackend* cb, CModule* module, AstFunction* function)
{
	StringBuffer& parentBuffer = GetSelectedBuffer(cb, module);
	StringBuffer& buffer = GetGlobalBuffer(cb, module);
	SelectBuffer(cb, module, buffer);

	GenType(cb, module, function->returnType);
	buffer << ' ' << function->mangledName << '(';
	for (int i = 0; i < function->paramTypes.size; i++)
	{
		GenType(cb, module, function->paramTypes[i]);
		if (function->paramNames[i])
			buffer << ' ' << function->paramNames[i];

		if (i < function->paramTypes.size - 1 || function->varArgs)
			buffer << ", ";
	}
	if (function->varArgs)
		buffer << "...";
	buffer << ");\n\n";

	SelectBuffer(cb, module, parentBuffer);
}

static void GenFuncCall(CBackend* cb, CModule* module, AstFuncCall* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	bool isMethodCall = false;

	if (expression->function)
	{
		// If the called function is defined below the current function or from a different module, generate a declaration for it
		//if (expression->function->functionID > module->currentFunction->functionID || expression->function->module != expression->module)
		{
			GenFunctionHeader(cb, module, expression->function);
		}
		buffer << expression->function->mangledName;
	}
	else if (expression->calleeExpr)
	{
		SnekAssert(expression->calleeExpr->type->typeKind = TYPE_KIND_FUNCTION, "");

		isMethodCall = expression->calleeExpr->type->functionType.isMethod;

		GenExpression(cb, module, expression->calleeExpr);
	}
	else
	{
		SnekAssert(false);
	}

	buffer << '(';

	if (isMethodCall)
	{
		SnekAssert(expression->calleeExpr->exprKind == EXPR_KIND_DOT_OPERATOR, "");

		AstExpression* object = ((AstDotOperator*)expression->calleeExpr)->operand;
		GenExpression(cb, module, object);
		buffer << ", ";
	}

	for (int i = 0; i < expression->arguments.size; i++)
	{
		GenExpression(cb, module, expression->arguments[i]);
		if (i < expression->arguments.size - 1)
		{
			buffer << ", ";
		}
	}
	buffer << ')';
}

static void GenSubscriptOperator(CBackend* cb, CModule* module, AstSubscriptOperator* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	if (expression->operand)
	{
		if (expression->arguments.size == 1)
		{
			GenExpression(cb, module, expression->operand);
			buffer << '[';
			GenExpression(cb, module, expression->arguments[0]);
			buffer << ']';
		}
		else
		{
			// TODO operator overloads
			SnekAssert(false);
		}
	}
	else
	{
		SnekAssert(false);
	}
}

static void GenDotOperator(CBackend* cb, CModule* module, AstDotOperator* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	if (expression->operand)
	{
		if (expression->operand->type->typeKind == TYPE_KIND_STRUCT)
		{
			GenExpression(cb, module, expression->operand);
			buffer << '.' << expression->name;
		}
		else if (expression->operand->type->typeKind == TYPE_KIND_CLASS)
		{
			GenExpression(cb, module, expression->operand);
			buffer << "->" << expression->name;
		}
		else if (expression->operand->type->typeKind == TYPE_KIND_POINTER)
		{
			GenExpression(cb, module, expression->operand);
			buffer << "->" << expression->name;
		}
		else if (expression->operand->type->typeKind == TYPE_KIND_ARRAY)
		{
			GenExpression(cb, module, expression->operand);
			buffer << '.' << expression->name;
		}
	}
	else
	{
		SnekAssert(false);
	}
}

static void GenCast(CBackend* cb, CModule* module, AstCast* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << '(';
	GenType(cb, module, expression->castType);
	buffer << ')';
	GenExpression(cb, module, expression->value);
}

static void GenMalloc(CBackend* cb, CModule* module, AstMalloc* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	if (expression->hasArguments)
	{
		AstClass* classDecl = ((AstNamedType*)expression->mallocType)->classDecl;

		buffer << classDecl->name << "__" << classDecl->name << "(malloc(sizeof(struct " << classDecl->name << "))";
		for (int i = 0; i < expression->arguments.size; i++)
		{
			buffer << ", ";
			GenExpression(cb, module, expression->arguments[i]);
		}
		buffer << ')';
	}
	else
	{
		buffer << "malloc(sizeof(";
		GenType(cb, module, expression->mallocType);
		buffer << ')';
		if (expression->count)
		{
			buffer << " * ";
			GenExpression(cb, module, expression->count);
		}
		buffer << ')';
	}
}

static void GenUnaryOperator(CBackend* cb, CModule* module, AstUnaryOperator* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	if (!expression->position)
	{
		switch (expression->operatorType)
		{
		case UNARY_OPERATOR_NOT: buffer << '!'; break;
		case UNARY_OPERATOR_NEGATE: buffer << '-'; break;
		case UNARY_OPERATOR_REFERENCE: buffer << '&'; break;
		case UNARY_OPERATOR_DEREFERENCE: buffer << '*'; break;

		case UNARY_OPERATOR_INCREMENT: buffer << "++"; break;
		case UNARY_OPERATOR_DECREMENT: buffer << "--"; break;

		default:
			SnekAssert(false);
			break;
		}
	}
	GenExpression(cb, module, expression->operand);
	if (expression->position)
	{
		switch (expression->operatorType)
		{
		case UNARY_OPERATOR_INCREMENT: buffer << "++"; break;
		case UNARY_OPERATOR_DECREMENT: buffer << "--"; break;

		default:
			SnekAssert(false);
			break;
		}
	}
}

static void GenBinaryOperator(CBackend* cb, CModule* module, AstBinaryOperator* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	GenExpression(cb, module, expression->left);
	buffer << ' ';

	switch (expression->operatorType)
	{
	case BINARY_OPERATOR_ADD: buffer << '+'; break;
	case BINARY_OPERATOR_SUB: buffer << '-'; break;
	case BINARY_OPERATOR_MUL: buffer << '*'; break;
	case BINARY_OPERATOR_DIV: buffer << '/'; break;
	case BINARY_OPERATOR_MOD: buffer << '%'; break;

	case BINARY_OPERATOR_EQ: buffer << "=="; break;
	case BINARY_OPERATOR_NE: buffer << "!="; break;
	case BINARY_OPERATOR_LT: buffer << '<'; break;
	case BINARY_OPERATOR_GT: buffer << '>'; break;
	case BINARY_OPERATOR_LE: buffer << "<="; break;
	case BINARY_OPERATOR_GE: buffer << ">="; break;
	case BINARY_OPERATOR_AND: buffer << "&&"; break;
	case BINARY_OPERATOR_OR: buffer << "||"; break;
	case BINARY_OPERATOR_BITWISE_AND: buffer << '&'; break;
	case BINARY_OPERATOR_BITWISE_OR: buffer << '|'; break;
	case BINARY_OPERATOR_BITWISE_XOR: buffer << '^'; break;
	case BINARY_OPERATOR_BITSHIFT_LEFT: buffer << "<<"; break;
	case BINARY_OPERATOR_BITSHIFT_RIGHT: buffer << ">>"; break;

	case BINARY_OPERATOR_ASSIGN: buffer << '='; break;
	case BINARY_OPERATOR_ADD_ASSIGN: buffer << "+="; break;
	case BINARY_OPERATOR_SUB_ASSIGN: buffer << "-="; break;
	case BINARY_OPERATOR_MUL_ASSIGN: buffer << "*="; break;
	case BINARY_OPERATOR_DIV_ASSIGN: buffer << "/="; break;
	case BINARY_OPERATOR_MOD_ASSIGN: buffer << "%="; break;
	case BINARY_OPERATOR_BITSHIFT_LEFT_ASSIGN: buffer << "<<="; break;
	case BINARY_OPERATOR_BITSHIFT_RIGHT_ASSIGN: buffer << ">>"; break;
	case BINARY_OPERATOR_BITWISE_AND_ASSIGN: buffer << "&="; break;
	case BINARY_OPERATOR_BITWISE_OR_ASSIGN: buffer << "|"; break;
	case BINARY_OPERATOR_BITWISE_XOR_ASSIGN: buffer << "^="; break;
	case BINARY_OPERATOR_REF_ASSIGN: buffer << "= &"; break;

	default:
		SnekAssert(false);
		break;
	}

	buffer << ' ';
	GenExpression(cb, module, expression->right);
}

static void GenTernaryOperator(CBackend* cb, CModule* module, AstTernaryOperator* expression)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	GenExpression(cb, module, expression->condition);
	buffer << " ? ";
	GenExpression(cb, module, expression->thenValue);
	buffer << " : ";
	GenExpression(cb, module, expression->elseValue);
}

static void GenExpression(CBackend* cb, CModule* module, AstExpression* expression)
{
	switch (expression->exprKind)
	{
	case EXPR_KIND_INTEGER_LITERAL:
		GenIntegerLiteral(cb, module, (AstIntegerLiteral*)expression);
		break;
	case EXPR_KIND_FP_LITERAL:
		GenFPLiteral(cb, module, (AstFPLiteral*)expression);
		break;
	case EXPR_KIND_BOOL_LITERAL:
		GenBoolLiteral(cb, module, (AstBoolLiteral*)expression);
		break;
	case EXPR_KIND_CHARACTER_LITERAL:
		GenCharacterLiteral(cb, module, (AstCharacterLiteral*)expression);
		break;
	case EXPR_KIND_NULL_LITERAL:
		GenNullLiteral(cb, module, (AstNullLiteral*)expression);
		break;
	case EXPR_KIND_STRING_LITERAL:
		GenStringLiteral(cb, module, (AstStringLiteral*)expression);
		break;
	case EXPR_KIND_IDENTIFIER:
		GenIdentifier(cb, module, (AstIdentifier*)expression);
		break;
	case EXPR_KIND_COMPOUND:
		GenCompoundExpression(cb, module, (AstCompoundExpression*)expression);
		break;

	case EXPR_KIND_FUNC_CALL:
		GenFuncCall(cb, module, (AstFuncCall*)expression);
		break;
	case EXPR_KIND_SUBSCRIPT_OPERATOR:
		GenSubscriptOperator(cb, module, (AstSubscriptOperator*)expression);
		break;
	case EXPR_KIND_DOT_OPERATOR:
		GenDotOperator(cb, module, (AstDotOperator*)expression);
		break;
	case EXPR_KIND_CAST:
		GenCast(cb, module, (AstCast*)expression);
		break;
	case EXPR_KIND_MALLOC:
		GenMalloc(cb, module, (AstMalloc*)expression);
		break;

	case EXPR_KIND_UNARY_OPERATOR:
		GenUnaryOperator(cb, module, (AstUnaryOperator*)expression);
		break;
	case EXPR_KIND_BINARY_OPERATOR:
		GenBinaryOperator(cb, module, (AstBinaryOperator*)expression);
		break;
	case EXPR_KIND_TERNARY_OPERATOR:
		GenTernaryOperator(cb, module, (AstTernaryOperator*)expression);
		break;

	default:
		SnekAssert(false);
		break;
	}
}

static void GenStatement(CBackend* cb, CModule* module, AstStatement* statement);

static void GenCompoundStatement(CBackend* cb, CModule* module, AstCompoundStatement* statement)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << '{';
	PushIndentation(cb, module);

	for (int i = 0; i < statement->statements.size; i++)
	{
		NewLine(cb, module);
		GenStatement(cb, module, statement->statements[i]);

		/*
		if (module->queuedStatements.size > 0)
		{
			for (int j = 0; j < module->queuedStatements.size; j++)
			{
				NewLine(cb, module);
				buffer << module->queuedStatements[j];
				delete module->queuedStatements[j];
			}
			module->queuedStatements.clear();
		}
		*/

		if (i < statement->statements.size - 1)
			NewLine(cb, module);
	}

	PopIndentation(cb, module);
	NewLine(cb, module);
	buffer << '}';
}

static void GenVarDeclStatement(CBackend* cb, CModule* module, AstVarDeclStatement* statement)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	GenType(cb, module, statement->type);

	for (int i = 0; i < statement->declarators.size; i++)
	{
		AstVarDeclarator* declarator = &statement->declarators[i];
		buffer << ' ' << declarator->name;
		if (declarator->value)
		{
			buffer << " = ";
			GenExpression(cb, module, declarator->value);
		}
		if (i < statement->declarators.size - 1)
			buffer << ',';
	}

	buffer << ';';
}

static void GenExprStatement(CBackend* cb, CModule* module, AstExprStatement* statement)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	GenExpression(cb, module, statement->expr);
	buffer << ';';
}

static void GenIfStatement(CBackend* cb, CModule* module, AstIfStatement* statement)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "if (";
	GenExpression(cb, module, statement->condition);
	buffer << ") { ";
	GenStatement(cb, module, statement->thenStatement);
	buffer << " }";

	if (statement->elseStatement)
	{
		buffer << " else { ";
		GenStatement(cb, module, statement->elseStatement);
		buffer << " }";
	}
}

static void GenForLoop(CBackend* cb, CModule* module, AstForLoop* statement)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "for (int32 " << statement->iteratorName << " = ";
	GenExpression(cb, module, statement->startValue);
	buffer << "; " << statement->iteratorName << " <= ";
	GenExpression(cb, module, statement->endValue);
	buffer << "; " << statement->iteratorName << " += " << statement->direction << ") { ";
	GenStatement(cb, module, statement->body);
	buffer << " }";
}

static void GenBreak(CBackend* cb, CModule* module, AstBreak* statement)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);
	buffer << "break;";
}

static void GenContinue(CBackend* cb, CModule* module, AstContinue* statement)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);
	buffer << "continue;";
}

static void GenReturn(CBackend* cb, CModule* module, AstReturn* statement)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);
	buffer << "return";

	if (statement->value)
	{
		buffer << ' ';
		GenExpression(cb, module, statement->value);
	}

	buffer << ';';
}

static void GenStatement(CBackend* cb, CModule* module, AstStatement* statement)
{
	switch (statement->statementKind)
	{
	case STATEMENT_KIND_COMPOUND:
		GenCompoundStatement(cb, module, (AstCompoundStatement*)statement);
		break;
	case STATEMENT_KIND_VAR_DECL:
		GenVarDeclStatement(cb, module, (AstVarDeclStatement*)statement);
		break;
	case STATEMENT_KIND_EXPR:
		GenExprStatement(cb, module, (AstExprStatement*)statement);
		break;
	case STATEMENT_KIND_IF:
		GenIfStatement(cb, module, (AstIfStatement*)statement);
		break;
	case STATEMENT_KIND_FOR:
		GenForLoop(cb, module, (AstForLoop*)statement);
		break;
	case STATEMENT_KIND_BREAK:
		GenBreak(cb, module, (AstBreak*)statement);
		break;
	case STATEMENT_KIND_CONTINUE:
		GenContinue(cb, module, (AstContinue*)statement);
		break;
	case STATEMENT_KIND_RETURN:
		GenReturn(cb, module, (AstReturn*)statement);
		break;

	default:
		SnekAssert(false);
		break;
	}
}

static void GenFunction(CBackend* cb, CModule* module, AstFunction* decl)
{
	StringBuffer& buffer = GetFunctionBuffer(cb, module, decl->functionID);
	SelectBuffer(cb, module, buffer);

	module->currentFunction = decl;

	GenType(cb, module, decl->returnType);
	buffer << ' ' << decl->mangledName << '(';
	for (int i = 0; i < decl->paramTypes.size; i++)
	{
		GenType(cb, module, decl->paramTypes[i]);
		if (decl->paramNames[i])
			buffer << ' ' << decl->paramNames[i];

		if (i < decl->paramTypes.size - 1 || decl->varArgs)
			buffer << ", ";
	}
	if (decl->varArgs)
		buffer << "...";
	buffer << ')';
	if (decl->body)
	{
		buffer << " {";
		GenStatement(cb, module, decl->body);
		buffer << "}";
	}
	else
		buffer << ';';
}

static void GenStructHeader(CBackend* cb, CModule* module, AstStruct* decl)
{
	StringBuffer& parentBuffer = GetSelectedBuffer(cb, module);
	StringBuffer& buffer = GetGlobalBuffer(cb, module);
	SelectBuffer(cb, module, buffer);

	buffer << "struct " << decl->name << " {";

	for (int i = 0; i < decl->fields.size; i++)
	{
		buffer << "\n\t";
		GenType(cb, module, decl->fields[i].type);
		buffer << ' ' << decl->fields[i].name << ';';
	}

	buffer << "\n};\n\n";

	SelectBuffer(cb, module, parentBuffer);
}

static void GenStruct(CBackend* cb, CModule* module, AstStruct* decl)
{
	StringBuffer& parentBuffer = GetSelectedBuffer(cb, module);
	StringBuffer& buffer = GetGlobalBuffer(cb, module);
	SelectBuffer(cb, module, buffer);

	buffer << "struct " << decl->name << " {";

	for (int i = 0; i < decl->fields.size; i++)
	{
		buffer << "\n\t";
		GenType(cb, module, decl->fields[i].type);
		buffer << ' ' << decl->fields[i].name << ';';
	}

	buffer << "\n};\n\n";

	SelectBuffer(cb, module, parentBuffer);
}

static void GenClassMethodHeader(CBackend* cb, CModule* module, AstFunction* method, AstClass* classDecl)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "\n";
	GenType(cb, module, method->returnType);
	buffer << ' ' << classDecl->name << "__" << method->name << '(';

	GenTypeID(cb, module, classDecl->type);
	buffer << " this";
	if (method->paramTypes.size > 0)
		buffer << ", ";

	for (int i = 0; i < method->paramTypes.size; i++)
	{
		GenType(cb, module, method->paramTypes[i]);
		buffer << ' ' << method->paramNames[i];
		if (i < method->paramTypes.size - 1)
			buffer << ", ";
	}
	buffer << ");";
}

static void GenClassConstructorHeader(CBackend* cb, CModule* module, AstFunction* constructor, AstClass* classDecl)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "\n";
	GenType(cb, module, constructor->returnType);
	buffer << ' ' << classDecl->name << "__" << classDecl->name << '(';

	GenTypeID(cb, module, classDecl->type);
	buffer << " this";
	if (constructor->paramTypes.size > 0)
		buffer << ", ";

	for (int i = 0; i < constructor->paramTypes.size; i++)
	{
		GenType(cb, module, constructor->paramTypes[i]);
		buffer << ' ' << constructor->paramNames[i];
		if (i < constructor->paramTypes.size - 1)
			buffer << ", ";
	}
	buffer << ");";
}

static void GenClassHeader(CBackend* cb, CModule* module, AstClass* decl)
{
	StringBuffer& parentBuffer = GetSelectedBuffer(cb, module);
	StringBuffer& buffer = GetGlobalBuffer(cb, module);
	SelectBuffer(cb, module, buffer);

	buffer << "struct " << decl->name << " {";

	for (int i = 0; i < decl->fields.size; i++)
	{
		buffer << "\n\t";
		GenType(cb, module, decl->fields[i].type);
		buffer << ' ' << decl->fields[i].name << ';';
	}

	buffer << "\n};";

	for (int i = 0; i < decl->methods.size; i++)
	{
		GenClassMethodHeader(cb, module, decl->methods[i], decl);
	}

	if (decl->constructor)
		GenClassConstructorHeader(cb, module, decl->constructor, decl);

	buffer << "\n\n";

	SelectBuffer(cb, module, parentBuffer);
}

static void GenClassMethod(CBackend* cb, CModule* module, AstFunction* method, AstClass* classDecl)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "\n";
	GenType(cb, module, method->returnType);
	buffer << ' ' << classDecl->name << "__" << method->name << '(';

	GenTypeID(cb, module, classDecl->type);
	buffer << " this";
	if (method->paramTypes.size > 0)
		buffer << ", ";

	for (int i = 0; i < method->paramTypes.size; i++)
	{
		GenType(cb, module, method->paramTypes[i]);
		buffer << ' ' << method->paramNames[i];
		if (i < method->paramTypes.size - 1)
			buffer << ", ";
	}
	buffer << ") {";
	GenStatement(cb, module, method->body);
	buffer << "}";
}

static void GenClassConstructor(CBackend* cb, CModule* module, AstFunction* constructor, AstClass* classDecl)
{
	StringBuffer& buffer = GetSelectedBuffer(cb, module);

	buffer << "\n";
	GenType(cb, module, constructor->returnType);
	buffer << ' ' << classDecl->name << "__" << classDecl->name << '(';

	GenTypeID(cb, module, classDecl->type);
	buffer << " this";
	if (constructor->paramTypes.size > 0)
		buffer << ", ";

	for (int i = 0; i < constructor->paramTypes.size; i++)
	{
		GenType(cb, module, constructor->paramTypes[i]);
		buffer << ' ' << constructor->paramNames[i];
		if (i < constructor->paramTypes.size - 1)
			buffer << ", ";
	}
	buffer << ") {";
	GenStatement(cb, module, constructor->body);
	buffer << " return this; }";
}

static void GenClass(CBackend* cb, CModule* module, AstClass* decl)
{
	StringBuffer& parentBuffer = GetSelectedBuffer(cb, module);
	StringBuffer& buffer = GetGlobalBuffer(cb, module);
	SelectBuffer(cb, module, buffer);

	buffer << "struct " << decl->name << " {";

	for (int i = 0; i < decl->fields.size; i++)
	{
		buffer << "\n\t";
		GenType(cb, module, decl->fields[i].type);
		buffer << ' ' << decl->fields[i].name << ';';
	}

	buffer << "\n};";

	for (int i = 0; i < decl->methods.size; i++)
	{
		GenClassMethod(cb, module, decl->methods[i], decl);
	}

	if (decl->constructor)
		GenClassConstructor(cb, module, decl->constructor, decl);

	buffer << "\n\n";

	SelectBuffer(cb, module, parentBuffer);
}

static void GenImport(CBackend* cb, CModule* module, AstImport* decl)
{
}

static void GenBuiltInDecls(CBackend* cb, CModule* module)
{
	StringBuffer& buffer = GetGlobalBuffer(cb, module);

	/*
	buffer << "typedef char int8;\n";
	buffer << "typedef short int16;\n";
	buffer << "typedef int int32;\n";
	buffer << "typedef long long int64;\n";
	buffer << "typedef long long int128;\n";

	buffer << "typedef unsigned char uint8;\n";
	buffer << "typedef unsigned short uint16;\n";
	buffer << "typedef unsigned int uint32;\n";
	buffer << "typedef unsigned long long uint64;\n";
	buffer << "typedef unsigned long long uint128;\n";

	buffer << "typedef float float32;\n";
	buffer << "typedef double float64;\n";
	buffer << "typedef double float80;\n";
	buffer << "typedef double float128;\n";

	buffer << "typedef _Bool bool;\n";
	*/

	buffer << "typedef struct { unsigned int len; char c; }* string;\n";
	buffer << "\n";

	buffer << "#define true 1\n";
	buffer << "#define false 0\n";

	buffer << "#define NULL 0\n";
	buffer << "\n";

	buffer << "void* malloc(unsigned int size);\n";
	buffer << "void free(void* ptr);\n";
	buffer << "string __new_string(const char* ptr);\n";

	buffer << "\n\n";
}

static void GenModule(CBackend* cb, CModule* module, AstFile* ast)
{
	SelectBuffer(cb, module, GetGlobalBuffer(cb, module));
	GenBuiltInDecls(cb, module);

	for (int i = 0; i < ast->imports.size; i++)
	{
		GenImport(cb, module, ast->imports[i]);
	}
	for (int i = 0; i < ast->structs.size; i++)
	{
		GenStruct(cb, module, ast->structs[i]);
	}
	for (int i = 0; i < ast->classes.size; i++)
	{
		GenClass(cb, module, ast->classes[i]);
	}
	for (int i = 0; i < ast->functions.size; i++)
	{
		GenFunction(cb, module, ast->functions[i]);
	}
}

static void TCCErrorFunc(void* opaque, const char* msg)
{
	CBackend* cb = (CBackend*)opaque;
	cb->context->msgCallback(MESSAGE_TYPE_ERROR, msg);
}

static void OutputCFile(const char* src, const char* name, const char* buildFolder)
{
	char path[64];
	strcpy(path, buildFolder);
	strcpy(path + strlen(path), "/");
	strcpy(path + strlen(path), name);
	strcpy(path + strlen(path), ".c");
	WriteTextFile(src, path);
}

bool CBackendCompile(CBackend* cb, AstFile** asts, int numModules, const char* filename, const char* buildFolder)
{
	tcc_set_error_func(cb->tcc, cb, TCCErrorFunc);
	tcc_set_output_type(cb->tcc, TCC_OUTPUT_EXE);
	tcc_add_library_path(cb->tcc, ".");
	tcc_set_options(cb->tcc, "g");

	cb->modules = new CModule[numModules];
	cb->numModules = numModules;

	tcc_compile_string(cb->tcc,
		"typedef struct { unsigned int len; char c; }* string;\n"
		"\n"
		"unsigned long strlen(const char* str);\n"
		"void* malloc(unsigned long size);\n"
		"void* memcpy(void* dst, const void* src, unsigned long n);\n"
		"\n"
		"string __new_string(const char* ptr) {\n"
		"unsigned long len = strlen(ptr);\n"
		"string str = malloc(sizeof(struct { unsigned int len; char c; }) + len);\n"
		"str->len = len;\n"
		"char* buffer = &str->c;\n"
		"memcpy(buffer, ptr, len);\n"
		"buffer[len] = '\\0';\n"
		"return str;\n"
		"}\n"
	);

	for (int i = 0; i < numModules; i++)
	{
		CModule module = {};

		module.ast = asts[i];
		module.currentFunction = NULL;

		module.globalBuffer = CreateStringBuffer(64);

		int numFunctions = asts[i]->functions.size;
		module.functionBuffers = CreateList<StringBuffer>(numFunctions);
		module.functionBuffers.resize(numFunctions);
		for (int i = 0; i < numFunctions; i++)
		{
			module.functionBuffers[i] = CreateStringBuffer(8);
		}

		module.indentation = 0;
		module.queuedStatements = CreateList<char*>();

		cb->modules[i] = module;
	}

	// TODO multithreading
	for (int i = 0; i < numModules; i++)
	{
		CModule* module = &cb->modules[i];
		AstFile* ast = asts[i];
		GenModule(cb, module, ast);
	}

	bool success = true;
	for (int i = 0; i < numModules; i++)
	{
		CModule* module = &cb->modules[i];
		AstFile* ast = asts[i];

		module->globalBuffer << "\n";
		for (int i = 0; i < module->functionBuffers.size; i++)
		{
			module->globalBuffer << module->functionBuffers[i].buffer << "\n\n";
		}

		OutputCFile(module->globalBuffer.buffer, ast->name, buildFolder);
	}

	for (int i = 0; i < numModules; i++)
	{
		CModule* module = &cb->modules[i];

		if (tcc_compile_string(cb->tcc, module->globalBuffer.buffer) == -1)
		{
			success = false;
		}
	}

	for (int i = 0; i < numModules; i++)
	{
		CModule* module = &cb->modules[i];
		DestroyStringBuffer(module->globalBuffer);
		for (int j = 0; j < module->functionBuffers.size; j++)
		{
			DestroyStringBuffer(module->functionBuffers[j]);
		}
	}

	for (int i = 0; i < cb->context->linkerFiles.size; i++)
	{
		tcc_add_file(cb->tcc, cb->context->linkerFiles[i].path);
	}

	if (success)
		success = tcc_output_file(cb->tcc, filename) == 0;

	delete cb->modules;

	return success;
}

