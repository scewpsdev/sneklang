#include "Statement.h"

#include "Expression.h"


namespace AST
{
	Statement::Statement(File* file, const SourceLocation& location, StatementType type)
		: Element(file, location), type(type)
	{
	}

	NoOpStatement::NoOpStatement(File* file, const SourceLocation& location)
		: Statement(file, location, StatementType::NoOp)
	{
	}

	Element* NoOpStatement::copy()
	{
		return new NoOpStatement(file, location);
	}

	ExpressionStatement::ExpressionStatement(File* file, const SourceLocation& location, Expression* expression)
		: Statement(file, location, StatementType::Expression), expression(expression)
	{
	}

	ExpressionStatement::~ExpressionStatement()
	{
		if (expression)
			delete expression;
	}

	Element* ExpressionStatement::copy()
	{
		return new ExpressionStatement(file, location, (Expression*)expression->copy());
	}

	CompoundStatement::CompoundStatement(File* file, const SourceLocation& location, const List<Statement*>& statements)
		: Statement(file, location, StatementType::Compound), statements(statements)
	{
	}

	CompoundStatement::~CompoundStatement()
	{
		for (int i = 0; i < statements.size; i++)
		{
			if (statements[i])
				delete statements[i];
		}
		DestroyList(statements);
	}

	Element* CompoundStatement::copy()
	{
		List<Statement*> statementsCopy = CreateList<Statement*>(statements.size);
		for (int i = 0; i < statements.size; i++)
			statementsCopy.add((Statement*)statements[i]->copy());

		return new CompoundStatement(file, location, statementsCopy);
	}

	VariableDeclarator::VariableDeclarator(File* file, const SourceLocation& location, char* name, Expression* value)
		: Element(file, location), name(name), value(value)
	{
	}

	VariableDeclarator::~VariableDeclarator()
	{
		if (name)
			delete name;
		if (value)
			delete value;
	}

	Element* VariableDeclarator::copy()
	{
		return new VariableDeclarator(file, location, _strdup(name), value ? (Expression*)value->copy() : nullptr);
	}

	VariableDeclaration::VariableDeclaration(File* file, const SourceLocation& location, Type* type, bool isConstant, List<VariableDeclarator*>& declarators)
		: Statement(file, location, StatementType::VariableDeclaration), varType(type), isConstant(isConstant), declarators(declarators)
	{
	}

	VariableDeclaration::~VariableDeclaration()
	{
		delete varType;
		for (int i = 0; i < declarators.size; i++)
		{
			if (declarators[i])
				delete declarators[i];
		}
		DestroyList(declarators);
	}

	Element* VariableDeclaration::copy()
	{
		List<VariableDeclarator*> declaratorsCopy = CreateList<VariableDeclarator*>(declarators.size);
		for (int i = 0; i < declarators.size; i++)
			declaratorsCopy.add((VariableDeclarator*)declarators[i]->copy());

		return new VariableDeclaration(file, location, (Type*)varType->copy(), isConstant, declaratorsCopy);
	}

	IfStatement::IfStatement(File* file, const SourceLocation& location, Expression* condition, Statement* thenStatement, Statement* elseStatement)
		: Statement(file, location, StatementType::If), condition(condition), thenStatement(thenStatement), elseStatement(elseStatement)
	{
	}

	IfStatement::~IfStatement()
	{
		if (condition)
			delete condition;
		if (thenStatement)
			delete thenStatement;
		if (elseStatement)
			delete elseStatement;
	}

	Element* IfStatement::copy()
	{
		return new IfStatement(file, location, (Expression*)condition->copy(), (Statement*)thenStatement->copy(), elseStatement ? (Statement*)elseStatement->copy() : nullptr);
	}

	WhileLoop::WhileLoop(File* file, const SourceLocation& location, Expression* condition, Statement* body)
		: Statement(file, location, StatementType::While), condition(condition), body(body)
	{
	}

	WhileLoop::~WhileLoop()
	{
		if (condition)
			delete condition;
		if (body)
			delete body;
	}

	Element* WhileLoop::copy()
	{
		return new WhileLoop(file, location, (Expression*)condition->copy(), (Statement*)body->copy());
	}

	ForLoop::ForLoop(File* file, const SourceLocation& location, Identifier* iteratorName, Expression* startValue, Expression* endValue, Expression* deltaValue, bool includeEndValue, Statement* body)
		: Statement(file, location, StatementType::For), iteratorName(iteratorName), startValue(startValue), endValue(endValue), deltaValue(deltaValue), includeEndValue(includeEndValue), body(body)
	{
	}

	ForLoop::~ForLoop()
	{
		if (iteratorName)
			delete iteratorName;
		if (startValue)
			delete startValue;
		if (endValue)
			delete endValue;
		if (deltaValue)
			delete deltaValue;
		if (body)
			delete body;
	}

	Element* ForLoop::copy()
	{
		return new ForLoop(file, location, (Identifier*)iteratorName->copy(), (Expression*)startValue->copy(), (Expression*)endValue->copy(), deltaValue ? (Expression*)deltaValue->copy() : nullptr, includeEndValue, (Statement*)body->copy());
	}

	Break::Break(File* file, const SourceLocation& location)
		: Statement(file, location, StatementType::Break)
	{
	}

	Element* Break::copy()
	{
		return new Break(file, location);
	}

	Continue::Continue(File* file, const SourceLocation& location)
		: Statement(file, location, StatementType::Continue)
	{
	}

	Element* Continue::copy()
	{
		return new Continue(file, location);
	}

	Return::Return(File* file, const SourceLocation& location, Expression* value)
		: Statement(file, location, StatementType::Return), value(value)
	{
	}

	Return::~Return()
	{
		if (value)
			delete value;
	}

	Element* Return::copy()
	{
		return new Return(file, location, value ? (Expression*)value->copy() : nullptr);
	}

	Defer::Defer(File* file, const SourceLocation& location, Statement* statement)
		: Statement(file, location, StatementType::Defer), statement(statement)
	{
	}

	Defer::~Defer()
	{
		if (statement)
			delete statement;
	}

	Element* Defer::copy()
	{
		return new Defer(file, location, (Statement*)statement->copy());
	}

	Free::Free(File* file, const SourceLocation& location, const List<Expression*>& values)
		: Statement(file, location, StatementType::Free), values(values)
	{
	}

	Free::~Free()
	{
		for (int i = 0; i < values.size; i++)
		{
			if (values[i])
				delete values[i];
		}
		DestroyList(values);
	}

	Element* Free::copy()
	{
		List<Expression*> valuesCopy = CreateList<Expression*>(values.size);
		for (int i = 0; i < values.size; i++)
			valuesCopy.add((Expression*)values[i]->copy());

		return new Free(file, location, valuesCopy);
	}
}
