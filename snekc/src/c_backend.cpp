#include "c_backend.h"

#include "ast.h"
#include "stringbuffer.h"
#include "log.h"
#include "snek.h"
#include "file.h"

#include <libtcc.h>


struct CModule
{
	StringBuffer buffer;
	int indentation;
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
	StringBuffer& buffer = module->buffer;

	buffer << "\n";
	for (int i = 0; i < module->indentation; i++)
		buffer << '\t';
}

static void GenType(CBackend* cb, CModule* module, AstType* type);

static void GenVoidType(CBackend* cb, CModule* module, AstVoidType* type)
{
	StringBuffer& buffer = module->buffer;

	buffer << "void";
}

static void GenIntegerType(CBackend* cb, CModule* module, AstIntegerType* type)
{
	StringBuffer& buffer = module->buffer;

	if (!type->isSigned)
		buffer << 'u';

	buffer << "int" << type->bitWidth;
}

static void GenFPType(CBackend* cb, CModule* module, AstFPType* type)
{
	StringBuffer& buffer = module->buffer;

	buffer << "float" << type->bitWidth;
}

static void GenBoolType(CBackend* cb, CModule* module, AstBoolType* type)
{
	StringBuffer& buffer = module->buffer;

	buffer << "bool";
}

static void GenPointerType(CBackend* cb, CModule* module, AstPointerType* type)
{
	StringBuffer& buffer = module->buffer;

	GenType(cb, module, type->elementType);
	buffer << '*';
}

static void GenType(CBackend* cb, CModule* module, AstType* type)
{
	switch (type->typeKind)
	{
	case TYPE_KIND_VOID:
		GenVoidType(cb, module, (AstVoidType*)type);
		break;
	case TYPE_KIND_INTEGER:
		GenIntegerType(cb, module, (AstIntegerType*)type);
		break;
	case TYPE_KIND_FP:
		GenFPType(cb, module, (AstFPType*)type);
		break;
	case TYPE_KIND_BOOL:
		GenBoolType(cb, module, (AstBoolType*)type);
		break;
	case TYPE_KIND_POINTER:
		GenPointerType(cb, module, (AstPointerType*)type);
		break;

	default:
		SnekAssert(false, "");
		break;
	}
}

static void GenExpression(CBackend* cb, CModule* module, AstExpression* expression);

static void GenIntegerLiteral(CBackend* cb, CModule* module, AstIntegerLiteral* expression)
{
	StringBuffer& buffer = module->buffer;

	buffer << (unsigned long)expression->value;
}

static void GenFPLiteral(CBackend* cb, CModule* module, AstFPLiteral* expression)
{
	StringBuffer& buffer = module->buffer;

	StringBufferAppend(buffer, expression->value);
}

static void GenBoolLiteral(CBackend* cb, CModule* module, AstBoolLiteral* expression)
{
	StringBuffer& buffer = module->buffer;

	buffer << expression->value;
}

static void GenCharacterLiteral(CBackend* cb, CModule* module, AstCharacterLiteral* expression)
{
	StringBuffer& buffer = module->buffer;

	buffer << (int)expression->value;
}

static void GenNullLiteral(CBackend* cb, CModule* module, AstNullLiteral* expression)
{
	StringBuffer& buffer = module->buffer;

	buffer << "NULL";
}

static void GenBinaryOperation(CBackend* cb, CModule* module, AstBinaryOperator* expression)
{
	StringBuffer& buffer = module->buffer;

	GenExpression(cb, module, expression->left);
	buffer << ' ';

	// TODO
	SnekAssert(false, "");

	buffer << ' ';
	GenExpression(cb, module, expression->right);
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
	case EXPR_KIND_BINARY_OPERATOR:
		GenBinaryOperation(cb, module, (AstBinaryOperator*)expression);
		break;

	default:
		SnekAssert(false, "");
		break;
	}
}

static void GenStatement(CBackend* cb, CModule* module, AstStatement* statement);

static void GenCompoundStatement(CBackend* cb, CModule* module, AstCompoundStatement* statement)
{
	StringBuffer& buffer = module->buffer;

	buffer << '{';
	PushIndentation(cb, module);
	NewLine(cb, module);

	for (int i = 0; i < statement->statements.size; i++)
	{
		GenStatement(cb, module, statement->statements[i]);
		if (i < statement->statements.size - 1)
			NewLine(cb, module);
	}

	PopIndentation(cb, module);
	NewLine(cb, module);
	buffer << '}';
}

static void GenVarDeclStatement(CBackend* cb, CModule* module, AstVarDeclStatement* statement)
{
	StringBuffer& buffer = module->buffer;

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
	StringBuffer& buffer = module->buffer;

	GenExpression(cb, module, statement->expr);
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

	default:
		SnekAssert(false, "");
		break;
	}
}

static void GenFuncDecl(CBackend* cb, CModule* module, AstFunction* decl)
{
	StringBuffer& buffer = module->buffer;

	GenType(cb, module, decl->returnType);
	buffer << ' ' << decl->mangledName << '(';

	buffer << ')';
	if (decl->body)
	{
		buffer << " { ";
		//PushIndentation(cb, module);
		//NewLine(cb, module);

		GenStatement(cb, module, decl->body);

		//PopIndentation(cb, module);
		//NewLine(cb, module);
		buffer << " }";
	}
	else
		buffer << ';';
}

static void GenDeclaration(CBackend* cb, CModule* module, AstDeclaration* decl)
{
	switch (decl->declKind)
	{
	case DECL_KIND_FUNC:
		GenFuncDecl(cb, module, (AstFunction*)decl);
		break;

	default:
		SnekAssert(false, "");
		break;
	}
}

static void GenBuiltInDecls(CBackend* cb, CModule* module)
{
	StringBuffer& buffer = module->buffer;

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

	buffer << "#define bool _Bool\n";
	buffer << "#define true 1\n";
	buffer << "#define false 0\n";

	buffer << "#define NULL 0\n";

	buffer << "\n\n";
}

static void GenModule(CBackend* cb, CModule* module, AstModule* ast)
{
	GenBuiltInDecls(cb, module);

	for (int i = 0; i < ast->declarations.size; i++)
	{
		GenDeclaration(cb, module, ast->declarations[i]);
		NewLine(cb, module);
		NewLine(cb, module);
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

bool CBackendCompile(CBackend* cb, AstModule** asts, int numModules, const char* filename, const char* buildFolder)
{
	tcc_set_error_func(cb->tcc, cb, TCCErrorFunc);
	tcc_set_output_type(cb->tcc, TCC_OUTPUT_EXE);
	tcc_add_library_path(cb->tcc, ".");
	tcc_set_options(cb->tcc, "g");

	cb->modules = new CModule[numModules];
	cb->numModules = numModules;

	for (int i = 0; i < numModules; i++)
	{
		CModule module = {};
		module.buffer = CreateStringBuffer(64);
		module.indentation = 0;
		cb->modules[i] = module;
	}

	// TODO multithreading
	for (int i = 0; i < numModules; i++)
	{
		CModule* module = &cb->modules[i];
		AstModule* ast = asts[i];
		GenModule(cb, module, ast);

		OutputCFile(module->buffer.buffer, ast->name, buildFolder);
	}

	for (int i = 0; i < numModules; i++)
		tcc_compile_string(cb->tcc, cb->modules[i].buffer.buffer);

	for (int i = 0; i < numModules; i++)
		DestroyStringBuffer(cb->modules[i].buffer);

	tcc_output_file(cb->tcc, filename);

	delete cb->modules;

	return true;
}
