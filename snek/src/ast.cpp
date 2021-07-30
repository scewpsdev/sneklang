#include "ast.h"

#include <stdlib.h>


AstModule* CreateAst(char* name, int moduleID)
{
	AstModule* ast = new AstModule();

	ast->name = name;
	ast->moduleID = moduleID;

	ast->blocks = CreateList<AstElementBlock*>();
	ast->index = 0;
	ListAdd(ast->blocks, (AstElementBlock*)malloc(sizeof(AstElementBlock)));

	ast->declarations = CreateList<AstDeclaration*>();

	return ast;
}

static AstElement* CreateAstElement(AstModule* module, const InputState& inputState, int size)
{
	int index = module->index;
	int relativeIdx = index % ELEMENT_BLOCK_SIZE;
	AstElementBlock* block = module->blocks[module->blocks.size - 1];

	if (relativeIdx + size >= ELEMENT_BLOCK_SIZE)
	{
		AstElementBlock* newBlock = (AstElementBlock*)malloc(sizeof(AstElementBlock));
		ListAdd(module->blocks, newBlock);
		block = newBlock;

		index = ((index + size) / ELEMENT_BLOCK_SIZE) * ELEMENT_BLOCK_SIZE;
		relativeIdx = 0;
	}

	AstElement* element = (AstElement*)&block->elements[relativeIdx];
	module->index = index + size;

	memset(element, 0, size);

	return element;
}

AstType* CreateAstType(AstModule* module, const InputState& inputState, AstTypeKind typeKind)
{
	AstType* type = (AstType*)CreateAstElement(module, inputState, sizeof(AstTypeStorage));
	type->typeKind = typeKind;
	return type;
}

AstExpression* CreateAstExpression(AstModule* module, const InputState& inputState, AstExpressionKind exprKind)
{
	AstExpression* expression = (AstExpression*)CreateAstElement(module, inputState, sizeof(AstExpressionStorage));
	expression->exprKind = exprKind;
	return expression;
}

AstStatement* CreateAstStatement(AstModule* module, const InputState& inputState, AstStatementKind statementKind)
{
	AstStatement* statement = (AstStatement*)CreateAstElement(module, inputState, sizeof(AstStatementStorage));
	statement->statementKind = statementKind;
	return statement;
}

AstDeclaration* CreateAstDeclaration(AstModule* module, const InputState& inputState, AstDeclarationKind declKind)
{
	AstDeclaration* decl = (AstDeclaration*)CreateAstElement(module, inputState, sizeof(AstDeclarationStorage));
	decl->declKind = declKind;
	return decl;
}
