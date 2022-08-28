#pragma once

#include "utils/List.h"

#include <stdint.h>


#define NUM_PRIMITIVE_TYPES 64


struct Resolver;

namespace AST
{
	struct Struct;
	struct Class;
	struct Function;
	struct Declaration;
	struct Expression;

	enum class TypeKind : uint8_t;
}

typedef struct TypeData* TypeID;

enum class FloatingPointPrecision
{
	Half = 16,
	Single = 32,
	Double = 64,
	Quad = 128,
};

struct TypeData
{
	AST::TypeKind typeKind;
	union
	{
		struct {
			int bitWidth;
			bool isSigned;
		} integerType;
		struct {
			FloatingPointPrecision precision;
		} fpType;
		struct {
			const char* name;
			bool hasBody;
			int numFields;
			TypeID* fieldTypes;
			const char** fieldNames;

			AST::Struct* declaration;
		} structType;
		struct {
			const char* name;
			int numFields;
			TypeID* fieldTypes;
			const char** fieldNames;

			AST::Class* declaration;
		} classType;
		struct {
			const char* name;
			TypeID alias;

			AST::Declaration* declaration;
		} aliasType;
		struct {
			TypeID elementType;
		} pointerType;
		struct {
			TypeID returnType;
			int numParams;
			TypeID* paramTypes;
			bool varArgs;
			bool isMethod;
			TypeID instanceType;

			AST::Function* declaration;
		} functionType;
		struct {
			TypeID elementType;
			int length;
			//AstExpression* length;
		} arrayType;
		struct {
			int length;
		} stringType;
	};
};


void InitTypeData();

TypeID GetVoidType();
TypeID GetIntegerType(int bitWidth, bool isSigned);
TypeID GetFloatingPointType(FloatingPointPrecision precision);
TypeID GetBoolType();
TypeID GetStringType();
TypeID GetStructType(const char* structName, AST::Struct* declaration);
TypeID GetStructType(int numValues, TypeID* valueTypes);
TypeID GetClassType(const char* className, AST::Class* declaration);
TypeID GetAliasType(const char* name, AST::Declaration* declaration);

TypeID GetPointerType(TypeID elementType);
TypeID GetFunctionType(TypeID returnType, int numParams, TypeID* paramTypes, bool varArgs, bool isMethod, TypeID instanceType, AST::Function* declaration);
TypeID GetArrayType(TypeID elementType, int length);

TypeID UnwrapType(TypeID type);

bool CompareTypes(TypeID t1, TypeID t2);

const char* GetTypeString(TypeID type);

bool CanConvert(TypeID argType, TypeID paramType);
bool CanConvertImplicit(TypeID argType, TypeID paramType, bool argIsConstant);
