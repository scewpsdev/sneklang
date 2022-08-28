#pragma once

#include "Element.h"

#include "utils/List.h"

#include <stdint.h>


typedef struct TypeData* TypeID;

namespace AST
{
	struct Declaration;
	struct Expression;

	enum class TypeKind : uint8_t
	{
		Null = 0,

		Void,
		Integer,
		FloatingPoint,
		Boolean,
		NamedType,
		Struct,
		Class,
		Alias,
		Pointer,
		Function,
		Array,
		String,
	};

	struct Type : Element
	{
		TypeKind typeKind = TypeKind::Null;

		TypeID typeID = nullptr;


		Type(File* file, const SourceLocation& location, TypeKind typeKind);

		virtual Element* copy() override = 0;
	};

	struct VoidType : Type
	{
		VoidType(File* file, const SourceLocation& location);

		virtual Element* copy() override;
	};

	struct IntegerType : Type
	{
		int bitWidth;
		bool isSigned;


		IntegerType(File* file, const SourceLocation& location, int bitWidth, bool isSigned);

		virtual Element* copy() override;
	};

	struct FloatingPointType : Type
	{
		int bitWidth;


		FloatingPointType(File* file, const SourceLocation& location, int bitWidth);

		virtual Element* copy() override;
	};

	struct BooleanType : Type
	{
		BooleanType(File* file, const SourceLocation& location);

		virtual Element* copy() override;
	};

	struct NamedType : Type
	{
		char* name;

		bool hasGenericArgs;
		List<Type*> genericArgs;

		Declaration* declaration = nullptr;


		NamedType(File* file, const SourceLocation& location, char* name, bool hasGenericArgs, const List<Type*>& genericArgs);
		~NamedType();

		virtual Element* copy() override;
	};

	struct PointerType : Type
	{
		Type* elementType;


		PointerType(File* file, const SourceLocation& location, Type* elementType);
		virtual ~PointerType();

		virtual Element* copy() override;
	};

	struct FunctionType : Type
	{
		Type* returnType;
		List<Type*> paramTypes;
		bool varArgs;


		FunctionType(File* file, const SourceLocation& location, Type* returnType, const List<Type*>& paramTypes, bool varArgs);
		virtual ~FunctionType();

		virtual Element* copy() override;
	};

	struct ArrayType : Type
	{
		Type* elementType;
		Expression* length;


		ArrayType(File* file, const SourceLocation& location, Type* elementType, Expression* length);
		virtual ~ArrayType();

		virtual Element* copy() override;
	};

	struct StringType : Type
	{
		Expression* length;


		StringType(File* file, const SourceLocation& location, Expression* length);

		virtual Element* copy() override;
	};
}
