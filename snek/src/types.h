#pragma once

#include "list.h"

#include <stdint.h>


#define NUM_PRIMITIVE_TYPES 64


struct Resolver;
enum AstTypeKind;

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
			int numMembers;
			char** memberNames;
			TypeID* memberTypes;
		} structType;
		struct {
			TypeID elementType;
		} pointerType;
		struct {
			TypeID returnType;
		} functionType;
	};
};

typedef TypeData* TypeID;

struct TypeDataStorage
{
	TypeData typeDataArray[NUM_PRIMITIVE_TYPES];
};


void InitTypeData(Resolver* resolver);

TypeID GetVoidType(Resolver* resolver);
TypeID GetIntegerType(Resolver* resolver, int bitWidth, bool isSigned);
TypeID GetFPType(Resolver* resolver, int bitWidth);
TypeID GetBoolType(Resolver* resolver);
