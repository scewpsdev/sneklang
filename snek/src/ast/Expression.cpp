#include "Expression.h"

#include "utils/Log.h"

#include "semantics/Variable.h"

#include <stdint.h>


namespace AST
{
	Expression::Expression(File* file, const SourceLocation& location, ExpressionType type)
		: Element(file, location), type(type)
	{
		valueType = nullptr;
		lvalue = false;
	}

	bool Expression::isConstant()
	{
		return false;
	}

	bool Expression::isLiteral()
	{
		return false;
	}

	IntegerLiteral::IntegerLiteral(File* file, const SourceLocation& location, int64_t value)
		: Expression(file, location, ExpressionType::IntegerLiteral), value(value)
	{
	}

	Element* IntegerLiteral::copy()
	{
		return new IntegerLiteral(file, location, value);
	}

	bool IntegerLiteral::isConstant()
	{
		return true;
	}

	bool IntegerLiteral::isLiteral()
	{
		return true;
	}

	FloatingPointLiteral::FloatingPointLiteral(File* file, const SourceLocation& location, double value)
		: Expression(file, location, ExpressionType::FloatingPointLiteral), value(value)
	{
	}

	Element* FloatingPointLiteral::copy()
	{
		return new FloatingPointLiteral(file, location, value);
	}

	bool FloatingPointLiteral::isConstant()
	{
		return true;
	}

	bool FloatingPointLiteral::isLiteral()
	{
		return true;
	}

	BooleanLiteral::BooleanLiteral(File* file, const SourceLocation& location, bool value)
		: Expression(file, location, ExpressionType::BooleanLiteral), value(value)
	{
	}

	Element* BooleanLiteral::copy()
	{
		return new BooleanLiteral(file, location, value);
	}

	bool BooleanLiteral::isConstant()
	{
		return true;
	}

	bool BooleanLiteral::isLiteral()
	{
		return true;
	}

	CharacterLiteral::CharacterLiteral(File* file, const SourceLocation& location, uint32_t value)
		: Expression(file, location, ExpressionType::CharacterLiteral), value(value)
	{
	}

	Element* CharacterLiteral::copy()
	{
		return new CharacterLiteral(file, location, value);
	}

	bool CharacterLiteral::isConstant()
	{
		return true;
	}

	bool CharacterLiteral::isLiteral()
	{
		return true;
	}

	NullLiteral::NullLiteral(File* file, const SourceLocation& location)
		: Expression(file, location, ExpressionType::NullLiteral)
	{
	}

	Element* NullLiteral::copy()
	{
		return new NullLiteral(file, location);
	}

	bool NullLiteral::isConstant()
	{
		return true;
	}

	bool NullLiteral::isLiteral()
	{
		return true;
	}

	StringLiteral::StringLiteral(File* file, const SourceLocation& location, char* value, int length)
		: Expression(file, location, ExpressionType::StringLiteral), value(value), length(length)
	{
	}

	StringLiteral::~StringLiteral()
	{
		if (value)
			delete value;
	}

	Element* StringLiteral::copy()
	{
		return new StringLiteral(file, location, _strdup(value), length);
	}

	bool StringLiteral::isConstant()
	{
		return true;
	}

	bool StringLiteral::isLiteral()
	{
		return true;
	}

	InitializerList::InitializerList(File* file, const SourceLocation& location, Type* initializerTypeAST, const List<Expression*>& values)
		: Expression(file, location, ExpressionType::InitializerList), initializerTypeAST(initializerTypeAST), values(values)
	{
	}

	InitializerList::~InitializerList()
	{
		if (initializerTypeAST)
			delete initializerTypeAST;
		for (int i = 0; i < values.size; i++)
		{
			if (values[i])
				delete values[i];
		}
		DestroyList(values);
	}

	Element* InitializerList::copy()
	{
		List<Expression*> valuesCopy = CreateList<Expression*>(values.size);
		for (int i = 0; i < values.size; i++)
			valuesCopy.add((Expression*)values[i]->copy());

		return new InitializerList(file, location, (Type*)initializerTypeAST->copy(), valuesCopy);
	}

	bool InitializerList::isConstant()
	{
		for (int i = 0; i < values.size; i++)
		{
			if (!values[i]->isConstant())
				return false;
		}
		return true;
	}

	bool InitializerList::isLiteral()
	{
		return true;
	}

	Identifier::Identifier(File* file, const SourceLocation& location, char* name)
		: Expression(file, location, ExpressionType::Identifier), name(name)
	{
	}

	Identifier::~Identifier()
	{
		if (name)
			delete name;
	}

	Element* Identifier::copy()
	{
		return new Identifier(file, location, _strdup(name));
	}

	bool Identifier::isConstant()
	{
		if (variable)
			return variable->isConstant;
		if (functions.size > 0)
			return true;
		if (exprdefValue)
			return false; // Whether the value is constant depends on where it gets used
		if (enumValue)
			return true;

		SnekAssert(false);
		return false;
	}

	CompoundExpression::CompoundExpression(File* file, const SourceLocation& location, Expression* value)
		: Expression(file, location, ExpressionType::Compound), value(value)
	{
	}

	CompoundExpression::~CompoundExpression()
	{
		if (value)
			delete value;
	}

	Element* CompoundExpression::copy()
	{
		return new CompoundExpression(file, location, (Expression*)value->copy());
	}

	bool CompoundExpression::isConstant()
	{
		return value->isConstant();
	}

	FunctionCall::FunctionCall(File* file, const SourceLocation& location, Expression* callee, const List<Expression*>& arguments, bool hasGenericArgs, const List<Type*>& genericArgs)
		: Expression(file, location, ExpressionType::FunctionCall), callee(callee), arguments(arguments), hasGenericArgs(hasGenericArgs), genericArgs(genericArgs)
	{
	}

	FunctionCall::~FunctionCall()
	{
		if (callee)
			delete callee;
		for (int i = 0; i < arguments.size; i++)
		{
			if (arguments[i])
				delete arguments[i];
		}
		DestroyList(arguments);

		if (hasGenericArgs)
		{
			for (int i = 0; i < genericArgs.size; i++)
			{
				if (genericArgs[i])
					delete genericArgs[i];
			}
			DestroyList(genericArgs);
			delete function;
		}
	}

	Element* FunctionCall::copy()
	{
		List<Expression*> argumentsCopy = CreateList<Expression*>(arguments.size);
		for (int i = 0; i < arguments.size; i++)
			argumentsCopy.add((Expression*)arguments[i]->copy());

		List<Type*> genericArgsCopy = {};
		if (hasGenericArgs)
		{
			genericArgsCopy = CreateList<Type*>(genericArgs.size);
			for (int i = 0; i < genericArgs.size; i++)
				genericArgsCopy.add((Type*)genericArgs[i]->copy());
		}

		return new FunctionCall(file, location, (Expression*)callee->copy(), argumentsCopy, hasGenericArgs, genericArgsCopy);
	}

	SubscriptOperator::SubscriptOperator(File* file, const SourceLocation& location, Expression* operand, const List<Expression*>& arguments)
		: Expression(file, location, ExpressionType::SubscriptOperator), operand(operand), arguments(arguments)
	{
	}

	SubscriptOperator::~SubscriptOperator()
	{
		if (operand)
			delete operand;
		for (int i = 0; i < arguments.size; i++)
		{
			if (arguments[i])
				delete arguments[i];
		}
		DestroyList(arguments);
	}

	Element* SubscriptOperator::copy()
	{
		List<Expression*> argumentsCopy = CreateList<Expression*>(arguments.size);
		for (int i = 0; i < arguments.size; i++)
			argumentsCopy.add((Expression*)arguments[i]->copy());

		return new SubscriptOperator(file, location, (Expression*)operand->copy(), argumentsCopy);
	}

	DotOperator::DotOperator(File* file, const SourceLocation& location, Expression* operand, char* name)
		: Expression(file, location, ExpressionType::DotOperator), operand(operand), name(name)
	{
	}

	DotOperator::~DotOperator()
	{
		if (operand)
			delete operand;
		if (name)
			delete name;
	}

	Element* DotOperator::copy()
	{
		return new DotOperator(file, location, (Expression*)operand->copy(), _strdup(name));
	}

	Typecast::Typecast(File* file, const SourceLocation& location, Expression* value, Type* dstType)
		: Expression(file, location, ExpressionType::Typecast), value(value), dstType(dstType)
	{
	}

	Typecast::~Typecast()
	{
		if (value)
			delete value;
		if (dstType)
			delete dstType;
	}

	Element* Typecast::copy()
	{
		return new Typecast(file, location, (Expression*)value->copy(), (Type*)dstType->copy());
	}

	Sizeof::Sizeof(File* file, const SourceLocation& location, Type* dstType)
		: Expression(file, location, ExpressionType::Sizeof), dstType(dstType)
	{
	}

	Sizeof::~Sizeof()
	{
		if (dstType)
			delete dstType;
	}

	Element* Sizeof::copy()
	{
		return new Sizeof(file, location, (Type*)dstType->copy());
	}

	Malloc::Malloc(File* file, const SourceLocation& location, Type* dstType, Expression* count, bool malloc, bool hasArguments, const List<Expression*>& arguments)
		: Expression(file, location, ExpressionType::Malloc), dstType(dstType), count(count), malloc(malloc), hasArguments(hasArguments), arguments(arguments)
	{
	}

	Malloc::~Malloc()
	{
		if (dstType)
			delete dstType;
		if (count)
			delete count;
	}

	Element* Malloc::copy()
	{
		List<Expression*> argumentsCopy;
		if (hasArguments)
		{
			argumentsCopy = CreateList<Expression*>(arguments.size);
			for (int i = 0; i < arguments.size; i++)
				argumentsCopy.add((Expression*)arguments[i]->copy());
		}

		return new Malloc(file, location, (Type*)dstType->copy(), count ? (Expression*)count->copy() : nullptr, malloc, hasArguments, argumentsCopy);
	}

	UnaryOperator::UnaryOperator(File* file, const SourceLocation& location, Expression* operand, UnaryOperatorType operatorType, bool position)
		: Expression(file, location, ExpressionType::UnaryOperator), operand(operand), operatorType(operatorType), position(position)
	{
	}

	UnaryOperator::~UnaryOperator()
	{
		if (operand)
			delete operand;
	}

	Element* UnaryOperator::copy()
	{
		return new UnaryOperator(file, location, (Expression*)operand->copy(), operatorType, position);
	}

	bool UnaryOperator::isConstant()
	{
		return operand->isConstant();
	}

	BinaryOperator::BinaryOperator(File* file, const SourceLocation& location, Expression* left, Expression* right, BinaryOperatorType operatorType)
		: Expression(file, location, ExpressionType::BinaryOperator), left(left), right(right), operatorType(operatorType)
	{
	}

	BinaryOperator::~BinaryOperator()
	{
		if (left)
			delete left;
		if (right)
			delete right;
	}

	Element* BinaryOperator::copy()
	{
		return new BinaryOperator(file, location, (Expression*)left->copy(), (Expression*)right->copy(), operatorType);
	}

	bool BinaryOperator::isConstant()
	{
		return left->isConstant() && right->isConstant();
	}

	TernaryOperator::TernaryOperator(File* file, const SourceLocation& location, Expression* condition, Expression* thenValue, Expression* elseValue)
		: Expression(file, location, ExpressionType::TernaryOperator), condition(condition), thenValue(thenValue), elseValue(elseValue)
	{
	}

	TernaryOperator::~TernaryOperator()
	{
		if (condition)
			delete condition;
		if (thenValue)
			delete thenValue;
		if (elseValue)
			delete elseValue;
	}

	Element* TernaryOperator::copy()
	{
		return new TernaryOperator(file, location, (Expression*)condition->copy(), (Expression*)thenValue->copy(), (Expression*)elseValue->copy());
	}

	bool TernaryOperator::isConstant()
	{
		return condition->isConstant() && thenValue->isConstant() && elseValue->isConstant();
	}
}