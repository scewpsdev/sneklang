#include "Type.h"

#include "Resolver.h"
#include "utils/Log.h"
#include "utils/Stringbuffer.h"

#include "ast/File.h"
#include "ast/Declaration.h"
#include "ast/Statement.h"

#include <map>


struct TypeDataStorage
{
	TypeData primitiveTypes[NUM_PRIMITIVE_TYPES];

	List<TypeData*> structTypes;
	List<TypeData*> classTypes;
	List<TypeData*> aliasTypes;
	List<TypeData*> pointerTypes;
	List<TypeData*> functionTypes;
	List<TypeData*> arrayTypes;
	//std::map<int, TypeData*> stringTypes;

	std::map<TypeID, char*> typeStrings;
};

enum TypeDataIndex
{
	TYPE_DATA_INDEX_NULL = 0,

	TYPE_DATA_INDEX_VOID,
	TYPE_DATA_INDEX_INT8,
	TYPE_DATA_INDEX_INT16,
	TYPE_DATA_INDEX_INT32,
	TYPE_DATA_INDEX_INT64,
	TYPE_DATA_INDEX_UINT8,
	TYPE_DATA_INDEX_UINT16,
	TYPE_DATA_INDEX_UINT32,
	TYPE_DATA_INDEX_UINT64,

	TYPE_DATA_INDEX_FLOAT16,
	TYPE_DATA_INDEX_FLOAT32,
	TYPE_DATA_INDEX_FLOAT64,
	TYPE_DATA_INDEX_FLOAT128,

	TYPE_DATA_INDEX_BOOL,

	TYPE_DATA_INDEX_STRING,
};


TypeDataStorage types;


static TypeData CreateVoidTypeData()
{
	TypeData data = {};
	data.typeKind = AST::TypeKind::Void;
	return data;
}

static TypeData CreateIntegerTypeData(int bitWidth, bool isSigned)
{
	TypeData data = {};
	data.typeKind = AST::TypeKind::Integer;
	data.integerType.bitWidth = bitWidth;
	data.integerType.isSigned = isSigned;
	return data;
}

static TypeData CreateFPTypeData(FloatingPointPrecision precision)
{
	TypeData data = {};
	data.typeKind = AST::TypeKind::FloatingPoint;
	data.fpType.precision = precision;
	return data;
}

static TypeData CreateBoolTypeData()
{
	TypeData data = {};
	data.typeKind = AST::TypeKind::Boolean;
	return data;
}

static TypeData CreateStringTypeData()
{
	TypeData data = {};
	data.typeKind = AST::TypeKind::String;
	return data;
}

static char* TypeToString(TypeID type);

static void InitPrimitiveTypes()
{
	types.primitiveTypes[TYPE_DATA_INDEX_VOID] = CreateVoidTypeData();

	types.primitiveTypes[TYPE_DATA_INDEX_INT8] = CreateIntegerTypeData(8, true);
	types.primitiveTypes[TYPE_DATA_INDEX_INT16] = CreateIntegerTypeData(16, true);
	types.primitiveTypes[TYPE_DATA_INDEX_INT32] = CreateIntegerTypeData(32, true);
	types.primitiveTypes[TYPE_DATA_INDEX_INT64] = CreateIntegerTypeData(64, true);
	types.primitiveTypes[TYPE_DATA_INDEX_UINT8] = CreateIntegerTypeData(8, false);
	types.primitiveTypes[TYPE_DATA_INDEX_UINT16] = CreateIntegerTypeData(16, false);
	types.primitiveTypes[TYPE_DATA_INDEX_UINT32] = CreateIntegerTypeData(32, false);
	types.primitiveTypes[TYPE_DATA_INDEX_UINT64] = CreateIntegerTypeData(64, false);

	types.primitiveTypes[TYPE_DATA_INDEX_FLOAT16] = CreateFPTypeData(FloatingPointPrecision::Half);
	types.primitiveTypes[TYPE_DATA_INDEX_FLOAT32] = CreateFPTypeData(FloatingPointPrecision::Single);
	types.primitiveTypes[TYPE_DATA_INDEX_FLOAT64] = CreateFPTypeData(FloatingPointPrecision::Double);
	types.primitiveTypes[TYPE_DATA_INDEX_FLOAT128] = CreateFPTypeData(FloatingPointPrecision::Quad);

	types.primitiveTypes[TYPE_DATA_INDEX_BOOL] = CreateBoolTypeData();

	types.primitiveTypes[TYPE_DATA_INDEX_STRING] = CreateStringTypeData();


	types.typeStrings.emplace(GetVoidType(), TypeToString(GetVoidType()));

	types.typeStrings.emplace(GetIntegerType(8, true), TypeToString(GetIntegerType(8, true)));
	types.typeStrings.emplace(GetIntegerType(16, true), TypeToString(GetIntegerType(16, true)));
	types.typeStrings.emplace(GetIntegerType(32, true), TypeToString(GetIntegerType(32, true)));
	types.typeStrings.emplace(GetIntegerType(64, true), TypeToString(GetIntegerType(64, true)));
	types.typeStrings.emplace(GetIntegerType(8, false), TypeToString(GetIntegerType(8, false)));
	types.typeStrings.emplace(GetIntegerType(16, false), TypeToString(GetIntegerType(16, false)));
	types.typeStrings.emplace(GetIntegerType(32, false), TypeToString(GetIntegerType(32, false)));
	types.typeStrings.emplace(GetIntegerType(64, false), TypeToString(GetIntegerType(64, false)));

	types.typeStrings.emplace(GetFloatingPointType(FloatingPointPrecision::Half), TypeToString(GetFloatingPointType(FloatingPointPrecision::Half)));
	types.typeStrings.emplace(GetFloatingPointType(FloatingPointPrecision::Single), TypeToString(GetFloatingPointType(FloatingPointPrecision::Single)));
	types.typeStrings.emplace(GetFloatingPointType(FloatingPointPrecision::Double), TypeToString(GetFloatingPointType(FloatingPointPrecision::Double)));
	types.typeStrings.emplace(GetFloatingPointType(FloatingPointPrecision::Quad), TypeToString(GetFloatingPointType(FloatingPointPrecision::Quad)));

	types.typeStrings.emplace(GetBoolType(), TypeToString(GetBoolType()));

	//types.typeStrings.emplace(GetStringType(), TypeToString(GetStringType()));
}

void InitTypeData()
{
	InitPrimitiveTypes();

	types.structTypes = CreateList<TypeData*>();
	types.classTypes = CreateList<TypeData*>();
	types.aliasTypes = CreateList<TypeData*>();
	types.pointerTypes = CreateList<TypeData*>();
	types.functionTypes = CreateList<TypeData*>();
	types.arrayTypes = CreateList<TypeData*>();
}

TypeID GetVoidType()
{
	return &types.primitiveTypes[TYPE_DATA_INDEX_VOID];
}

TypeID GetIntegerType(int bitWidth, bool isSigned)
{
	if (isSigned)
	{
		switch (bitWidth)
		{
		case 8: return &types.primitiveTypes[TYPE_DATA_INDEX_INT8];
		case 16: return &types.primitiveTypes[TYPE_DATA_INDEX_INT16];
		case 32: return &types.primitiveTypes[TYPE_DATA_INDEX_INT32];
		case 64: return &types.primitiveTypes[TYPE_DATA_INDEX_INT64];
		}
	}
	else
	{
		switch (bitWidth)
		{
		case 8: return &types.primitiveTypes[TYPE_DATA_INDEX_UINT8];
		case 16: return &types.primitiveTypes[TYPE_DATA_INDEX_UINT16];
		case 32: return &types.primitiveTypes[TYPE_DATA_INDEX_UINT32];
		case 64: return &types.primitiveTypes[TYPE_DATA_INDEX_UINT64];
		}
	}
	SnekAssert(false);
	return NULL;
}

TypeID GetFloatingPointType(FloatingPointPrecision precision)
{
	switch (precision)
	{
	case FloatingPointPrecision::Half: return &types.primitiveTypes[TYPE_DATA_INDEX_FLOAT16];
	case FloatingPointPrecision::Single: return &types.primitiveTypes[TYPE_DATA_INDEX_FLOAT32];
	case FloatingPointPrecision::Double: return &types.primitiveTypes[TYPE_DATA_INDEX_FLOAT64];
	case FloatingPointPrecision::Quad: return &types.primitiveTypes[TYPE_DATA_INDEX_FLOAT128];
	default:
		SnekAssert(false);
		return NULL;
	}
}

TypeID GetBoolType()
{
	return &types.primitiveTypes[TYPE_DATA_INDEX_BOOL];
}

TypeID GetStringType()
{
	return &types.primitiveTypes[TYPE_DATA_INDEX_STRING];

	/*
	auto it = types.stringTypes.find(length);
	if (it != types.stringTypes.end())
	{
		return it->second;
	}
	else
	{
		TypeData* data = new TypeData;
		data->typeKind = AST::TypeKind::String;
		data->stringType.length = length;

		types.stringTypes.emplace(length, data);

		return data;
	}
	*/
}

TypeID GetStructType(const char* structName, AST::Struct* declaration)
{
	TypeData* data = new TypeData;
	data->typeKind = AST::TypeKind::Struct;

	data->structType.name = structName;
	data->structType.declaration = declaration;

	types.structTypes.add(data);

	return data;
}

TypeID GetStructType(int numValues, TypeID* valueTypes)
{
	TypeData* data = new TypeData;
	data->typeKind = AST::TypeKind::Struct;

	data->structType.name = NULL;
	data->structType.numFields = numValues;
	data->structType.fieldTypes = valueTypes;
	data->structType.fieldNames = NULL;
	data->structType.declaration = NULL;

	types.structTypes.add(data);

	return data;
}

TypeID GetClassType(const char* className, AST::Class* declaration)
{
	TypeData* data = new TypeData;
	data->typeKind = AST::TypeKind::Class;

	data->classType.name = className;
	data->classType.declaration = declaration;

	types.classTypes.add(data);

	return data;
}

TypeID GetAliasType(const char* name, AST::Declaration* declaration)
{
	TypeData* data = new TypeData;
	data->typeKind = AST::TypeKind::Alias;

	data->aliasType.name = name;
	data->aliasType.declaration = declaration;

	types.aliasTypes.add(data);

	return data;
}

TypeID GetPointerType(TypeID elementType)
{
	TypeData* data = new TypeData;
	data->typeKind = AST::TypeKind::Pointer;
	data->pointerType.elementType = elementType;

	types.pointerTypes.add(data);

	return data;
}

TypeID GetFunctionType(TypeID returnType, int numParams, TypeID* paramTypes, bool varArgs, bool isMethod, TypeID instanceType, AST::Function* declaration)
{
	TypeData* data = new TypeData;
	data->typeKind = AST::TypeKind::Function;
	data->functionType.returnType = returnType;
	data->functionType.numParams = numParams;
	data->functionType.paramTypes = paramTypes;
	data->functionType.varArgs = varArgs;
	data->functionType.isMethod = isMethod;
	data->functionType.instanceType = instanceType;
	data->functionType.declaration = declaration;

	types.functionTypes.add(data);

	return data;
}

TypeID GetArrayType(TypeID elementType, int length)
{
	TypeData* data = new TypeData;
	data->typeKind = AST::TypeKind::Array;
	data->arrayType.elementType = elementType;
	data->arrayType.length = length;

	types.arrayTypes.add(data);

	return data;
}

TypeID UnwrapType(TypeID type)
{
	while (type->typeKind == AST::TypeKind::Alias)
		type = type->aliasType.alias;
	return type;
}

bool CompareTypes(TypeID t1, TypeID t2)
{
	t1 = UnwrapType(t1);
	t2 = UnwrapType(t2);

	if (t1->typeKind != t2->typeKind)
		return false;

	switch (t1->typeKind)
	{
	case AST::TypeKind::Void:
		return true;
	case AST::TypeKind::Integer:
		return t1->integerType.bitWidth == t2->integerType.bitWidth && t1->integerType.isSigned == t2->integerType.isSigned;
	case AST::TypeKind::FloatingPoint:
		return t1->fpType.precision == t2->fpType.precision;
	case AST::TypeKind::Boolean:
		return true;
	case AST::TypeKind::Struct:
		if (strcmp(t1->structType.name, t2->structType.name) == 0 && t1->structType.numFields == t2->structType.numFields)
		{
			if (t1->structType.declaration->isGenericInstance != t2->structType.declaration->isGenericInstance)
				return false;

			if (t1->structType.declaration->isGenericInstance && t2->structType.declaration->isGenericInstance)
			{
				if (t1->structType.declaration->genericParams.size == t1->structType.declaration->genericParams.size)
				{
					for (int i = 0; i < t1->structType.declaration->genericTypeArguments.size; i++)
					{
						if (!CompareTypes(t1->structType.declaration->genericTypeArguments[i], t2->structType.declaration->genericTypeArguments[i]))
							return false;
					}
					return true;
				}
				else
				{
					return false;
				}
			}
			if (t1->structType.declaration != t2->structType.declaration)
				return false;
			/*
			for (int i = 0; i < t1->structType.numFields; i++)
			{
				if (!CompareTypes(t1->structType.fieldTypes[i], t2->structType.fieldTypes[i]))
					return false;
			}
			*/
			return true;
		}
		return false;
	case AST::TypeKind::Class:
		if (strcmp(t1->classType.name, t2->classType.name) == 0 && t1->classType.numFields == t2->classType.numFields)
		{
			for (int i = 0; i < t1->classType.numFields; i++)
			{
				if (!CompareTypes(t1->classType.fieldTypes[i], t2->classType.fieldTypes[i]))
					return false;
			}
			return true;
		}
		return false;
	case AST::TypeKind::Alias:
		if (strcmp(t1->aliasType.name, t2->aliasType.name) == 0)
		{
			if (CompareTypes(t1->aliasType.alias, t2->aliasType.alias))
				return true;
		}
		return false;
	case AST::TypeKind::Pointer:
		return CompareTypes(t1->pointerType.elementType, t2->pointerType.elementType);
	case AST::TypeKind::Function:
		if (CompareTypes(t1->functionType.returnType, t2->functionType.returnType) && t1->functionType.numParams == t2->functionType.numParams)
		{
			for (int i = 0; i < t1->functionType.numParams; i++)
			{
				if (!CompareTypes(t1->functionType.paramTypes[i], t2->functionType.paramTypes[i]))
					return false;
			}
			return true;
		}
		return false;
	case AST::TypeKind::Array:
		return CompareTypes(t1->arrayType.elementType, t2->arrayType.elementType) && t1->arrayType.length == t2->arrayType.length;
	case AST::TypeKind::String:
		return true;

	default:
		SnekAssert(false);
		return false;
	}
}

static char* TypeToString(TypeID type)
{
	switch (type->typeKind)
	{
	case AST::TypeKind::Void:
		return _strdup("void");
	case AST::TypeKind::Integer:
		if (type->integerType.isSigned)
		{
			switch (type->integerType.bitWidth)
			{
			case 8: return _strdup("int8");
			case 16: return _strdup("int16");
			case 32: return _strdup("int32");
			case 64: return _strdup("int64");
			default:
				SnekAssert(false);
				return NULL;
			}
		}
		else
		{
			switch (type->integerType.bitWidth)
			{
			case 8: return _strdup("uint8");
			case 16: return _strdup("uint16");
			case 32: return _strdup("uint32");
			case 64: return _strdup("uint64");
			default:
				SnekAssert(false);
				return NULL;
			}
		}
	case AST::TypeKind::FloatingPoint:
		switch (type->fpType.precision)
		{
		case FloatingPointPrecision::Half: return _strdup("float16");
		case FloatingPointPrecision::Single: return _strdup("float32");
		case FloatingPointPrecision::Double: return _strdup("float64");
		case FloatingPointPrecision::Quad: return _strdup("float128");
		}
	case AST::TypeKind::Boolean:
		return _strdup("bool");
	case AST::TypeKind::Struct:
	{
		StringBuffer result = CreateStringBuffer(8);
		StringBufferAppend(result, type->structType.name);

		if (type->structType.declaration && type->structType.declaration->isGenericInstance)
		{
			StringBufferAppend(result, '<');
			for (int i = 0; i < type->structType.declaration->genericTypeArguments.size; i++)
			{
				const char* argString = GetTypeString(type->structType.declaration->genericTypeArguments[i]);
				StringBufferAppend(result, argString);
				if (i < type->structType.declaration->genericTypeArguments.size - 1)
					StringBufferAppend(result, ',');
			}
			StringBufferAppend(result, '>');
		}

		return result.buffer;
	}
	case AST::TypeKind::Class:
		return _strdup(type->classType.name);
	case AST::TypeKind::Alias:
		return _strdup(type->aliasType.name);
	case AST::TypeKind::Pointer:
	{
		const char* elementTypeStr = GetTypeString(type->pointerType.elementType);
		int len = (int)strlen(elementTypeStr) + 1;
		char* str = new char[len + 1];
		strcpy(str, elementTypeStr);
		strcpy(str + strlen(str), "*");
		str[len] = 0;

		return str;
	}
	case AST::TypeKind::Function:
	{
		const char* returnTypeStr = GetTypeString(type->functionType.returnType);
		int len = type->functionType.returnType->typeKind != AST::TypeKind::Void ? (2 + 4 + (int)strlen(returnTypeStr)) : 2; // ' -> ', '()'

		for (int i = 0; i < type->functionType.numParams; i++)
		{
			const char* paramTypeStr = GetTypeString(type->functionType.paramTypes[i]);
			len += (int)strlen(paramTypeStr);
			if (i < type->functionType.numParams - 1 || type->functionType.varArgs)
				len += 2; // ', '
		}
		if (type->functionType.varArgs)
			len += 3; // '...'

		char* str = new char[len + 1];
		str[0] = 0;
		strcat(str, "(");

		for (int i = 0; i < type->functionType.numParams; i++)
		{
			const char* paramTypeStr = GetTypeString(type->functionType.paramTypes[i]);
			strcat(str, paramTypeStr);
			if (i < type->functionType.numParams - 1 || type->functionType.varArgs)
				strcat(str, ", ");
		}
		if (type->functionType.varArgs)
			strcat(str, "...");

		strcat(str, ")");

		if (type->functionType.returnType->typeKind != AST::TypeKind::Void)
		{
			strcat(str, " -> ");
			strcat(str, returnTypeStr);
		}

		return str;
	}
	case AST::TypeKind::Array:
	{
		const char* elementTypeStr = GetTypeString(type->arrayType.elementType);
		int len = (int)strlen(elementTypeStr) + 2; // []
		if (type->arrayType.length != -1)
		{
			int numDigits = type->arrayType.length == 0 ? 1 : (int)(log10(type->arrayType.length) + 1.001);
			len += numDigits;
		}

		char* str = new char[len + 1];
		str[0] = 0;
		strcat(str, elementTypeStr);
		strcat(str, "[");
		if (type->arrayType.length != -1)
			sprintf(str + strlen(str), "%d", type->arrayType.length);
		strcat(str, "]");

		return str;
	}
	case AST::TypeKind::String:
		return _strdup("string");

	default:
		SnekAssert(false);
		return NULL;
	}
}

const char* GetTypeString(TypeID type)
{
	auto entry = types.typeStrings.find(type);
	if (entry != types.typeStrings.end())
	{
		return entry->second;
	}
	else
	{
		char* str = TypeToString(type);
		types.typeStrings.emplace(type, str);
		return str;
	}
}

bool CanConvert(TypeID argType, TypeID paramType)
{
	if (CompareTypes(argType, paramType))
		return true;

	while (argType->typeKind == AST::TypeKind::Alias)
		argType = argType->aliasType.alias;
	while (paramType->typeKind == AST::TypeKind::Alias)
		paramType = paramType->aliasType.alias;

	if (argType->typeKind == AST::TypeKind::Integer)
	{
		if (paramType->typeKind == AST::TypeKind::Integer)
			return true;
		else if (paramType->typeKind == AST::TypeKind::Boolean)
			return true;
		else if (paramType->typeKind == AST::TypeKind::FloatingPoint)
			return true;
		else if (paramType->typeKind == AST::TypeKind::Pointer)
			return true;
	}
	else if (argType->typeKind == AST::TypeKind::FloatingPoint)
	{
		if (paramType->typeKind == AST::TypeKind::FloatingPoint)
			return true;
		else if (paramType->typeKind == AST::TypeKind::Integer)
			return true;
	}
	else if (argType->typeKind == AST::TypeKind::Boolean)
	{
		if (paramType->typeKind == AST::TypeKind::Integer)
			return true;
	}
	else if (argType->typeKind == AST::TypeKind::Pointer)
	{
		if (paramType->typeKind == AST::TypeKind::Pointer)
			return true;
		else if (paramType->typeKind == AST::TypeKind::Class)
			return true;
		else if (paramType->typeKind == AST::TypeKind::Function)
			return true;
		else if (paramType->typeKind == AST::TypeKind::Integer)
			return true;
		//else if (paramType->typeKind == AST::TypeKind::String)
		//	return true;
	}
	else if (argType->typeKind == AST::TypeKind::Function)
	{
		if (paramType->typeKind == AST::TypeKind::Function)
			return true;
		else if (paramType->typeKind == AST::TypeKind::Pointer)
			return true;
	}
	else if (argType->typeKind == AST::TypeKind::String)
	{
		//if (paramType->typeKind == AST::TypeKind::Pointer)
		//	return true;
	}

	return false;
}

bool CanConvertImplicit(TypeID argType, TypeID paramType, bool argIsConstant)
{
	argType = UnwrapType(argType);
	paramType = UnwrapType(paramType);

	if (CompareTypes(argType, paramType))
		return true;

	while (argType->typeKind == AST::TypeKind::Alias)
		argType = argType->aliasType.alias;
	while (paramType->typeKind == AST::TypeKind::Alias)
		paramType = paramType->aliasType.alias;

	if (argType->typeKind == AST::TypeKind::Integer && paramType->typeKind == AST::TypeKind::Integer)
	{
		if (argType->integerType.bitWidth == paramType->integerType.bitWidth)
			return true;
		else if (argType->integerType.bitWidth <= paramType->integerType.bitWidth)
			//return argIsConstant;
			return true;
	}
	else if (argType->typeKind == AST::TypeKind::Integer && paramType->typeKind == AST::TypeKind::Boolean)
	{
		return true;
	}
	else if (argType->typeKind == AST::TypeKind::Boolean && paramType->typeKind == AST::TypeKind::Integer)
	{
		if (argIsConstant)
			return true;
	}
	else if (argType->typeKind == AST::TypeKind::FloatingPoint && paramType->typeKind == AST::TypeKind::FloatingPoint)
	{
		if (argIsConstant)
			return true;
	}
	else if (argType->typeKind == AST::TypeKind::Pointer && paramType->typeKind == AST::TypeKind::Pointer)
	{
		if (/*argIsConstant || */argType->pointerType.elementType->typeKind == AST::TypeKind::Void || paramType->pointerType.elementType->typeKind == AST::TypeKind::Void)
			return true;
	}
	else if (argType->typeKind == AST::TypeKind::Array && paramType->typeKind == AST::TypeKind::Array)
	{
		if (CompareTypes(argType->arrayType.elementType, paramType->arrayType.elementType))
		{
			if (paramType->arrayType.length == -1 || argType->arrayType.length == paramType->arrayType.length)
				return true;
		}
		else if (CanConvertImplicit(argType->arrayType.elementType, paramType->arrayType.elementType, true) && argIsConstant)
		{
			if (argType->arrayType.length != -1)
			{
				if (paramType->arrayType.length != -1 && argType->arrayType.length == paramType->arrayType.length ||
					paramType->arrayType.length == -1)
					return true;
			}
		}
	}
	else if (argType->typeKind == AST::TypeKind::Pointer && paramType->typeKind == AST::TypeKind::Class)
	{
		if (argIsConstant)
			return true;
	}
	else if (argType->typeKind == AST::TypeKind::Pointer && paramType->typeKind == AST::TypeKind::Function)
	{
		if (argIsConstant)
			return true;
	}
	else if (argType->typeKind == AST::TypeKind::Pointer && paramType->typeKind == AST::TypeKind::String)
	{
		//if (argIsConstant)
		//return true;
	}
	else if (argType->typeKind == AST::TypeKind::String && paramType->typeKind == AST::TypeKind::Pointer && paramType->pointerType.elementType->typeKind == AST::TypeKind::Integer && paramType->pointerType.elementType->integerType.bitWidth == 8)
	{
		//if (argIsConstant)
		//return true;
	}

	return false;
}
