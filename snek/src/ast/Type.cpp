#include "Type.h"

#include "Expression.h"


namespace AST
{
	AST::Type::Type(File* file, const SourceLocation& location, TypeKind typeKind)
		: Element(file, location), typeKind(typeKind)
	{
	}

	AST::VoidType::VoidType(File* file, const SourceLocation& location)
		: Type(file, location, TypeKind::Void)
	{
	}

	Element* VoidType::copy()
	{
		return new VoidType(file, location);
	}

	AST::IntegerType::IntegerType(File* file, const SourceLocation& location, int bitWidth, bool isSigned)
		: Type(file, location, TypeKind::Integer), bitWidth(bitWidth), isSigned(isSigned)
	{
	}

	Element* AST::IntegerType::copy()
	{
		return new IntegerType(file, location, bitWidth, isSigned);
	}

	AST::FloatingPointType::FloatingPointType(File* file, const SourceLocation& location, int bitWidth)
		: Type(file, location, TypeKind::FloatingPoint), bitWidth(bitWidth)
	{
	}

	Element* FloatingPointType::copy()
	{
		return new FloatingPointType(file, location, bitWidth);
	}

	AST::BooleanType::BooleanType(File* file, const SourceLocation& location)
		: Type(file, location, TypeKind::Boolean)
	{
	}

	Element* BooleanType::copy()
	{
		return new BooleanType(file, location);
	}

	AST::NamedType::NamedType(File* file, const SourceLocation& location, char* name)
		: Type(file, location, TypeKind::NamedType), name(name)
	{
	}

	Element* NamedType::copy()
	{
		return new NamedType(file, location, _strdup(name));
	}

	AST::PointerType::PointerType(File* file, const SourceLocation& location, Type* elementType)
		: Type(file, location, TypeKind::Pointer), elementType(elementType)
	{
	}

	PointerType::~PointerType()
	{
		if (elementType)
			delete elementType;
	}

	Element* PointerType::copy()
	{
		return new PointerType(file, location, (Type*)elementType->copy());
	}

	AST::FunctionType::FunctionType(File* file, const SourceLocation& location, Type* returnType, const List<Type*>& paramTypes, bool varArgs)
		: Type(file, location, TypeKind::Function), returnType(returnType), paramTypes(paramTypes), varArgs(varArgs)
	{
	}

	FunctionType::~FunctionType()
	{
		if (returnType)
			delete returnType;
		for (int i = 0; i < paramTypes.size; i++)
		{
			if (paramTypes[i])
				delete paramTypes[i];
		}
		DestroyList(paramTypes);
	}

	Element* FunctionType::copy()
	{
		List<Type*> paramTypesCopy = CreateList<Type*>(paramTypes.size);
		for (int i = 0; i < paramTypes.size; i++)
			paramTypesCopy.add((Type*)paramTypes[i]->copy());

		return new FunctionType(file, location, (Type*)returnType->copy(), paramTypesCopy, varArgs);
	}

	AST::ArrayType::ArrayType(File* file, const SourceLocation& location, Type* elementType, Expression* length)
		: Type(file, location, TypeKind::Array), elementType(elementType), length(length)
	{
	}

	ArrayType::~ArrayType()
	{
		if (elementType)
			delete elementType;
		if (length)
			delete length;
	}

	Element* ArrayType::copy()
	{
		return new ArrayType(file, location, (Type*)elementType->copy(), length ? (Expression*)length->copy() : nullptr);
	}

	AST::StringType::StringType(File* file, const SourceLocation& location)
		: Type(file, location, TypeKind::String)
	{
	}

	Element* StringType::copy()
	{
		return new StringType(file, location);
	}
}
