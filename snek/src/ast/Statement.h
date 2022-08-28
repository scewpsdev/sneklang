#pragma once

#include "Element.h"
#include "Type.h"
#include "Expression.h"
#include "utils/List.h"

#include <stdint.h>


struct Variable;

namespace AST
{
	enum class StatementType : uint8_t
	{
		Null = 0,

		NoOp,
		Compound,
		Expression,
		VariableDeclaration,
		If,
		While,
		For,
		Break,
		Continue,
		Return,
		Defer,
		Free,
	};

	struct Statement : Element
	{
		StatementType type;


		Statement(File* file, const SourceLocation& location, StatementType type);

		virtual Element* copy() override = 0;
	};

	struct NoOpStatement : Statement
	{
		NoOpStatement(File* file, const SourceLocation& location);

		virtual Element* copy() override;
	};

	struct ExpressionStatement : Statement
	{
		Expression* expression;


		ExpressionStatement(File* file, const SourceLocation& location, Expression* expression);
		virtual ~ExpressionStatement();

		virtual Element* copy() override;
	};

	struct CompoundStatement : Statement
	{
		List<Statement*> statements;


		CompoundStatement(File* file, const SourceLocation& location, const List<Statement*>& statements);
		virtual ~CompoundStatement();

		virtual Element* copy() override;
	};

	struct VariableDeclarator : Element
	{
		char* name;
		Expression* value;

		Variable* variable = nullptr;


		VariableDeclarator(File* file, const SourceLocation& location, char* name, Expression* value);
		virtual ~VariableDeclarator();

		virtual Element* copy() override;
	};

	struct VariableDeclaration : Statement
	{
		Type* varType;
		bool isConstant;
		List<VariableDeclarator*> declarators;


		VariableDeclaration(File* file, const SourceLocation& location, Type* type, bool isConstant, List<VariableDeclarator*>& declarators);
		virtual ~VariableDeclaration();

		virtual Element* copy() override;
	};

	struct IfStatement : Statement
	{
		Expression* condition;
		Statement* thenStatement, * elseStatement;


		IfStatement(File* file, const SourceLocation& location, Expression* condition, Statement* thenStatement, Statement* elseStatement);
		virtual ~IfStatement();

		virtual Element* copy() override;
	};

	struct WhileLoop : Statement
	{
		Expression* condition;
		Statement* body;

		ControlFlowHandle breakHandle = nullptr;
		ControlFlowHandle continueHandle = nullptr;


		WhileLoop(File* file, const SourceLocation& location, Expression* condition, Statement* body);
		virtual ~WhileLoop();

		virtual Element* copy() override;
	};

	struct ForLoop : Statement
	{
		Identifier* iteratorName;
		Expression* startValue, * endValue, * deltaValue;
		bool includeEndValue;
		Statement* body;

		Variable* iterator = nullptr;
		int delta = 0;

		ControlFlowHandle breakHandle = nullptr;
		ControlFlowHandle continueHandle = nullptr;


		ForLoop(File* file, const SourceLocation& location, Identifier* iteratorName, Expression* startValue, Expression* endValue, Expression* deltaValue, bool includeEndValue, Statement* body);
		virtual ~ForLoop();

		virtual Element* copy() override;
	};

	struct Break : Statement
	{
		Statement* branchDst = nullptr;


		Break(File* file, const SourceLocation& location);

		virtual Element* copy() override;
	};

	struct Continue : Statement
	{
		Statement* branchDst = nullptr;


		Continue(File* file, const SourceLocation& location);

		virtual Element* copy() override;
	};

	struct Return : Statement
	{
		Expression* value;


		Return(File* file, const SourceLocation& location, Expression* value);
		virtual ~Return();

		virtual Element* copy() override;
	};

	struct Defer : Statement
	{
		Statement* statement;


		Defer(File* file, const SourceLocation& location, Statement* statement);
		virtual ~Defer();

		virtual Element* copy() override;
	};

	struct Free : Statement
	{
		List<Expression*> values;


		Free(File* file, const SourceLocation& location, const List<Expression*>& values);
		virtual ~Free();

		virtual Element* copy() override;
	};
}