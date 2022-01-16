#pragma once

#include "List.h"

#include <stdint.h>


#define NUM_PRIMITIVE_TYPES 64


struct Resolver;
struct AstExpression;

typedef struct TypeData* TypeID;

enum FPTypePrecision
{
	FP_PRECISION_HALF,
	FP_PRECISION_SINGLE,
	FP_PRECISION_DOUBLE,
	FP_PRECISION_FP128,
};

struct TypeData
{
	uint8_t typeKind;
	union
	{
		struct {
			int bitWidth;
			bool isSigned;
		} integerType;
		struct {
			FPTypePrecision precision;
		} fpType;
		struct {
			const char* name;
			bool hasBody;
			int numFields;
			TypeID* fieldTypes;
			const char** fieldNames;

			struct AstStruct* declaration;
		} structType;
		struct {
			const char* name;
			int numFields;
			TypeID* fieldTypes;
			const char** fieldNames;

			struct AstClass* declaration;
		} classType;
		struct {
			const char* name;
			TypeID alias;

			struct AstDeclaration* declaration;
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

			struct AstFunction* declaration;
		} functionType;
		struct {
			TypeID elementType;
			int length;
			//AstExpression* length;
		} arrayType;
	};
};


void InitTypeData();

TypeID GetVoidType();
TypeID GetIntegerType(int bitWidth, bool isSigned);
TypeID GetFPType(FPTypePrecision precision);
TypeID GetBoolType();
TypeID GetStringType();
TypeID GetStructType(const char* structName, struct AstStruct* declaration);
TypeID GetStructType(int numValues, TypeID* valueTypes);
TypeID GetClassType(const char* className, struct AstClass* declaration);
TypeID GetAliasType(const char* name, AstDeclaration* declaration);

TypeID GetPointerType(TypeID elementType);
TypeID GetFunctionType(TypeID returnType, int numParams, TypeID* paramTypes, bool varArgs, bool isMethod, struct AstFunction* declaration);
TypeID GetArrayType(TypeID elementType, int length);

TypeID UnwrapType(TypeID type);

bool CompareTypes(TypeID t1, TypeID t2);

const char* GetTypeString(TypeID type);
