#include "types.h"

#include "ast.h"
#include "resolver.h"


#define TYPE_DATA_INDEX_VOID_TYPE 1
#define TYPE_DATA_INDEX_INT8_TYPE 2
#define TYPE_DATA_INDEX_INT16_TYPE 3
#define TYPE_DATA_INDEX_INT32_TYPE 4
#define TYPE_DATA_INDEX_INT64_TYPE 5
#define TYPE_DATA_INDEX_UINT8_TYPE 6
#define TYPE_DATA_INDEX_UINT16_TYPE 7
#define TYPE_DATA_INDEX_UINT32_TYPE 8
#define TYPE_DATA_INDEX_UINT64_TYPE 9


static TypeData CreateVoidTypeData()
{
	TypeData data = {};
	data.typeKind = TYPE_KIND_VOID;
	return data;
}

static TypeData CreateIntegerTypeData(int bitWidth, bool isSigned)
{
	TypeData data = {};
	data.typeKind = TYPE_KIND_INTEGER;
	data.integerType.bitWidth = bitWidth;
	data.integerType.isSigned = isSigned;
	return data;
}

void InitTypeData(Resolver* resolver)
{
	resolver->types.typeDataArray[TYPE_DATA_INDEX_VOID_TYPE] = CreateVoidTypeData();
	resolver->types.typeDataArray[TYPE_DATA_INDEX_INT8_TYPE] = CreateIntegerTypeData(8, true);
	resolver->types.typeDataArray[TYPE_DATA_INDEX_INT16_TYPE] = CreateIntegerTypeData(16, true);
	resolver->types.typeDataArray[TYPE_DATA_INDEX_INT32_TYPE] = CreateIntegerTypeData(32, true);
	resolver->types.typeDataArray[TYPE_DATA_INDEX_INT64_TYPE] = CreateIntegerTypeData(64, true);
	resolver->types.typeDataArray[TYPE_DATA_INDEX_UINT8_TYPE] = CreateIntegerTypeData(8, false);
	resolver->types.typeDataArray[TYPE_DATA_INDEX_UINT16_TYPE] = CreateIntegerTypeData(16, false);
	resolver->types.typeDataArray[TYPE_DATA_INDEX_UINT32_TYPE] = CreateIntegerTypeData(32, false);
	resolver->types.typeDataArray[TYPE_DATA_INDEX_UINT64_TYPE] = CreateIntegerTypeData(64, false);
}

TypeID GetVoidType(Resolver* resolver)
{
	return &resolver->types.typeDataArray[TYPE_DATA_INDEX_VOID_TYPE];
}

TypeID GetIntegerType(Resolver* resolver, int bitWidth, bool isSigned)
{
	if (isSigned)
	{
		switch (bitWidth)
		{
		case 8: return &resolver->types.typeDataArray[TYPE_DATA_INDEX_INT8_TYPE];
		case 16: return &resolver->types.typeDataArray[TYPE_DATA_INDEX_INT16_TYPE];
		case 32: return &resolver->types.typeDataArray[TYPE_DATA_INDEX_INT32_TYPE];
		case 64: return &resolver->types.typeDataArray[TYPE_DATA_INDEX_INT64_TYPE];
		}
	}
	else
	{
		switch (bitWidth)
		{
		case 8: return &resolver->types.typeDataArray[TYPE_DATA_INDEX_UINT8_TYPE];
		case 16: return &resolver->types.typeDataArray[TYPE_DATA_INDEX_UINT16_TYPE];
		case 32: return &resolver->types.typeDataArray[TYPE_DATA_INDEX_UINT32_TYPE];
		case 64: return &resolver->types.typeDataArray[TYPE_DATA_INDEX_UINT64_TYPE];
		}
	}
	return NULL;
}

TypeID GetFPType(Resolver* resolver, int bitWidth)
{

}

TypeID GetBoolType(Resolver* resolver)
{

}
