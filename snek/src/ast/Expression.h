#pragma once

#include "Element.h"
#include "Type.h"
#include "Variable.h"

#include "semantics/Type.h"


namespace AST
{
	struct Module;
	struct Function;
	struct EnumValue;

	struct StructField;
	struct ClassField;


	enum class ExpressionType : uint8_t
	{
		Null = 0,

		IntegerLiteral,
		FloatingPointLiteral,
		BooleanLiteral,
		CharacterLiteral,
		NullLiteral,
		StringLiteral,
		StructLiteral,
		Identifier,
		Compound,

		FunctionCall,
		SubscriptOperator,
		DotOperator,
		Typecast,
		Sizeof,
		Malloc,

		UnaryOperator,
		BinaryOperator,
		TernaryOperator,
	};

	enum class UnaryOperatorType : uint8_t
	{
		Null = 0,

		Not,
		Negate,
		Reference,
		Dereference,

		Increment,
		Decrement,
	};

	enum class BinaryOperatorType : uint8_t
	{
		Null = 0,

		Add,
		Subtract,
		Multiply,
		Divide,
		Modulo,

		Equals,
		DoesNotEqual,
		LessThan,
		GreaterThan,
		LessThanEquals,
		GreaterThanEquals,
		LogicalAnd,
		LogicalOr,
		BitwiseAnd,
		BitwiseOr,
		BitwiseXor,
		BitshiftLeft,
		BitshiftRight,

		Assignment,
		PlusEquals,
		MinusEquals,
		TimesEquals,
		DividedByEquals,
		ModuloEquals,
		BitwiseAndEquals,
		BitwiseOrEquals,
		BitwiseXorEquals,
		BitshiftLeftEquals,
		BitshiftRightEquals,
		ReferenceAssignment,

		Ternary, // Technically not a binary operator, but it makes sense to have it here
	};

	struct Expression : Element
	{
		ExpressionType type;

		TypeID valueType;
		bool lvalue;


		Expression(File* file, const SourceLocation& location, ExpressionType type);

		virtual Element* copy() override = 0;
		virtual bool isConstant();
		virtual bool isLiteral();
	};

	struct IntegerLiteral : Expression
	{
		int64_t value = 0;


		IntegerLiteral(File* file, const SourceLocation& location, int64_t value);

		virtual Element* copy() override;
		virtual bool isConstant() override;
		virtual bool isLiteral() override;
	};

	struct FloatingPointLiteral : Expression
	{
		double value = 0.0;


		FloatingPointLiteral(File* file, const SourceLocation& location, double value);

		virtual Element* copy() override;
		virtual bool isConstant() override;
		virtual bool isLiteral() override;
	};

	struct BooleanLiteral : Expression
	{
		bool value;


		BooleanLiteral(File* file, const SourceLocation& location, bool value);

		virtual Element* copy() override;
		virtual bool isConstant() override;
		virtual bool isLiteral() override;
	};

	struct CharacterLiteral : Expression
	{
		uint32_t value;


		CharacterLiteral(File* file, const SourceLocation& location, uint32_t value);

		virtual Element* copy() override;
		virtual bool isConstant() override;
		virtual bool isLiteral() override;
	};

	struct NullLiteral : Expression
	{
		NullLiteral(File* file, const SourceLocation& location);

		virtual Element* copy() override;
		virtual bool isConstant() override;
		virtual bool isLiteral() override;
	};

	struct StringLiteral : Expression
	{
		char* value;
		int length;


		StringLiteral(File* file, const SourceLocation& location, char* value, int length);
		virtual ~StringLiteral();

		virtual Element* copy() override;
		virtual bool isConstant() override;
		virtual bool isLiteral() override;
	};

	struct StructLiteral : Expression
	{
		Type* structType;
		List<Expression*> values;


		StructLiteral(File* file, const SourceLocation& location, Type* structType, const List<Expression*>& values);
		virtual ~StructLiteral();

		virtual Element* copy() override;
		virtual bool isConstant() override;
		virtual bool isLiteral() override;
	};

	struct Identifier : Expression
	{
		char* name;

		Variable* variable = nullptr;
		Function* function = nullptr;
		Expression* exprdefValue = nullptr;
		EnumValue* enumValue = nullptr;


		Identifier(File* file, const SourceLocation& location, char* name);
		virtual ~Identifier();

		virtual Element* copy() override;
		virtual bool isConstant() override;
	};

	struct CompoundExpression : Expression
	{
		Expression* value;


		CompoundExpression(File* file, const SourceLocation& location, Expression* value);
		virtual ~CompoundExpression();

		virtual Element* copy() override;
		virtual bool isConstant() override;
	};

	struct FunctionCall : Expression
	{
		Expression* calleeExpr;
		List<Expression*> arguments;

		bool isGenericCall;
		List<Type*> genericArgs;

		Function* function = nullptr;
		bool isMethodCall = false;
		Expression* methodInstance = nullptr;


		FunctionCall(File* file, const SourceLocation& location, Expression* callee, const List<Expression*>& arguments, bool isGenericCall, const List<Type*>& genericArgs);
		virtual ~FunctionCall();

		virtual Element* copy() override;
	};

	struct SubscriptOperator : Expression
	{
		Expression* operand;
		List<Expression*> arguments;


		SubscriptOperator(File* file, const SourceLocation& location, Expression* operand, const List<Expression*>& arguments);
		virtual ~SubscriptOperator();

		virtual Element* copy() override;
	};

	struct DotOperator : Expression
	{
		Expression* operand;
		char* name;

		Module* ns = nullptr;
		Function* namespacedFunction = nullptr;
		Variable* namespacedVariable = nullptr;

		// structs
		StructField* structField = nullptr;
		Function* classMethod = nullptr;
		ValueHandle methodInstance = nullptr;

		// classes
		ClassField* classField = nullptr;

		// arrays
		int arrayField = -1;

		// strings
		int stringField = -1;


		DotOperator(File* file, const SourceLocation& location, Expression* operand, char* name);
		virtual ~DotOperator();

		virtual Element* copy() override;
	};

	struct Typecast : Expression
	{
		Expression* value;
		Type* dstType;


		Typecast(File* file, const SourceLocation& location, Expression* value, Type* dstType);
		virtual ~Typecast();

		virtual Element* copy() override;
	};

	struct Sizeof : Expression
	{
		Type* dstType;


		Sizeof(File* file, const SourceLocation& location, Type* dstType);
		virtual ~Sizeof();

		virtual Element* copy() override;
	};

	struct Malloc : Expression
	{
		Type* dstType;
		Expression* count;

		//bool hasArguments;
		//List<Expression*> arguments;


		Malloc(File* file, const SourceLocation& location, Type* dstType, Expression* count);
		virtual ~Malloc();

		virtual Element* copy() override;
	};

	struct UnaryOperator : Expression
	{
		Expression* operand;
		UnaryOperatorType operatorType;
		bool position;


		UnaryOperator(File* file, const SourceLocation& location, Expression* operand, UnaryOperatorType operatorType, bool position);
		virtual ~UnaryOperator();

		virtual Element* copy() override;
		virtual bool isConstant() override;
	};

	struct BinaryOperator : Expression
	{
		Expression* left, * right;
		BinaryOperatorType operatorType;


		BinaryOperator(File* file, const SourceLocation& location, Expression* left, Expression* right, BinaryOperatorType operatorType);
		virtual ~BinaryOperator();

		virtual Element* copy() override;
		virtual bool isConstant() override;
	};

	struct TernaryOperator : Expression
	{
		Expression* condition, * thenValue, * elseValue;


		TernaryOperator(File* file, const SourceLocation& location, Expression* condition, Expression* thenValue, Expression* elseValue);
		virtual ~TernaryOperator();

		virtual Element* copy() override;
		virtual bool isConstant() override;
	};
}
