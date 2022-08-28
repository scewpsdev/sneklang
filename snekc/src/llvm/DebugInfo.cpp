#include "DebugInfo.h"

#include "Debug.h"
#include "LLVMBackend.h"
#include "Types.h"
#include "utils/Log.h"
#include "ast/File.h"

#include <llvm-c/DebugInfo.h>


static LLVMMetadataRef int8Type;
static LLVMMetadataRef int16Type;
static LLVMMetadataRef int32Type;
static LLVMMetadataRef int64Type;
static LLVMMetadataRef int128Type;

static LLVMMetadataRef stringType;


void InitDebugInfo(LLVMBackend* llb, SkModule* module, const char* filename, const char* directory)
{
	const char* producer = "Sneklang Compiler";

	LLVMMetadataRef file = LLVMDIBuilderCreateFile(module->diBuilder, filename, strlen(filename), directory, strlen(directory));
	LLVMMetadataRef cu = LLVMDIBuilderCreateCompileUnit(module->diBuilder, LLVMDWARFSourceLanguageC, file, producer, strlen(producer), false, "", 0, 0, "", 0, LLVMDWARFEmissionFull, 0, false, false, "", 0, "", 0);

	unsigned int version = LLVMDebugMetadataVersion();
	LLVMAddModuleFlag(module->llvmModule, LLVMModuleFlagBehaviorWarning, "Debug Info Version", strlen("Debug Info Version"), LLVMValueAsMetadata(LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), version, false)));
	unsigned int codeView = 1;
	LLVMAddModuleFlag(module->llvmModule, LLVMModuleFlagBehaviorWarning, "CodeView", strlen("CodeView"), LLVMValueAsMetadata(LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), codeView, false)));

	module->diCompileUnit = cu;
	module->debugScopes.add(cu);


	// Int types
	{
		int8Type = LLVMDIBuilderCreateBasicType(module->diBuilder, "char", strlen("char"), 8, 8U, LLVMDIFlagZero);
		int16Type = LLVMDIBuilderCreateBasicType(module->diBuilder, "short", strlen("short"), 16, 5U, LLVMDIFlagZero);
		int32Type = LLVMDIBuilderCreateBasicType(module->diBuilder, "int", strlen("int"), 32, 5U, LLVMDIFlagZero);
		int64Type = LLVMDIBuilderCreateBasicType(module->diBuilder, "long", strlen("long"), 64, 5U, LLVMDIFlagZero);
		int128Type = LLVMDIBuilderCreateBasicType(module->diBuilder, "int128", strlen("int128"), 128, 5U, LLVMDIFlagZero);
	}

	// String type
	{
		LLVMTypeRef llvmType = GetStringType(llb);

		LLVMMetadataRef fieldTypes[2];

		{
			LLVMMetadataRef type = int32Type;

			uint64_t size = LLVMDITypeGetSizeInBits(type);
			uint64_t offset = LLVMOffsetOfElement(llb->targetData, llvmType, 0) * 8;
			fieldTypes[0] = LLVMDIBuilderCreateMemberType(
				module->diBuilder, NULL,
				"length", strlen("length"),
				nullptr,
				0,
				size,
				0,
				offset,
				LLVMDIFlagZero,
				type
			);
		}
		{
			LLVMMetadataRef type = LLVMDIBuilderCreatePointerType(module->diBuilder, int8Type, 64, 0, 0, NULL, 0);

			uint64_t size = LLVMDITypeGetSizeInBits(type);
			uint64_t offset = LLVMOffsetOfElement(llb->targetData, llvmType, 1) * 8;
			fieldTypes[1] = LLVMDIBuilderCreateMemberType(
				module->diBuilder, NULL,
				"buffer", strlen("buffer"),
				nullptr,
				0,
				size,
				0,
				offset,
				LLVMDIFlagZero,
				type
			);
		}

		int size = (int)LLVMSizeOfTypeInBits(llb->targetData, llvmType);

		stringType = LLVMDIBuilderCreateStructType(
			module->diBuilder, module->diCompileUnit,
			"string", strlen("string"),
			nullptr,
			0,
			size,
			0,
			LLVMDIFlagZero,
			NULL,
			fieldTypes, 2,
			0,
			NULL,
			NULL,
			0
		);
	}
}

void CompleteDebugInfo(LLVMBackend* llb, SkModule* module)
{
	module->debugScopes.removeAt(module->debugScopes.size - 1);

	LLVMDIBuilderFinalize(module->diBuilder);
}

void DebugInfoEmitSourceLocation(LLVMBackend* llb, SkModule* module, LLVMBuilderRef builder, const AST::SourceLocation& location)
{
	LLVMMetadataRef diScope = module->debugScopes[module->debugScopes.size - 1];
	LLVMMetadataRef debugLocation = LLVMDIBuilderCreateDebugLocation(llb->llvmContext, location.line, location.col, diScope, NULL);
	LLVMSetCurrentDebugLocation2(builder, debugLocation);
}

void DebugInfoEmitNullLocation(LLVMBackend* llb, SkModule* module, LLVMBuilderRef builder)
{
	LLVMSetCurrentDebugLocation2(builder, NULL);
}

void DebugInfoEmitSourceLocation(LLVMBackend* llb, SkModule* module, AST::Element* element)
{
	DebugInfoEmitSourceLocation(llb, module, module->builder, element->location);
}

void DebugInfoPushScope(LLVMBackend* llb, SkModule* module, LLVMMetadataRef scope)
{
	module->debugScopes.add(scope);
}

LLVMMetadataRef DebugInfoPopScope(LLVMBackend* llb, SkModule* module)
{
	SnekAssert(module->debugScopes.size > 0);

	LLVMMetadataRef scope = module->debugScopes[module->debugScopes.size - 1];
	module->debugScopes.removeAt(module->debugScopes.size - 1);

	return scope;
}

static LLVMMetadataRef DebugInfoCreateType(LLVMBackend* llb, SkModule* module, TypeID type)
{
	switch (type->typeKind)
	{
	case AST::TypeKind::Void:
		return NULL;
	case AST::TypeKind::Integer:
		switch (type->integerType.bitWidth)
		{
		case 8: return LLVMDIBuilderCreateBasicType(module->diBuilder, "char", strlen("char"), 8, 8U, LLVMDIFlagZero);
		case 16: return LLVMDIBuilderCreateBasicType(module->diBuilder, "short", strlen("short"), 16, 5U, LLVMDIFlagZero);
		case 32: return LLVMDIBuilderCreateBasicType(module->diBuilder, "int", strlen("int"), 32, 5U, LLVMDIFlagZero);
		case 64: return LLVMDIBuilderCreateBasicType(module->diBuilder, "long", strlen("long"), 64, 5U, LLVMDIFlagZero);
		case 128: return LLVMDIBuilderCreateBasicType(module->diBuilder, "int128", strlen("int128"), 128, 5U, LLVMDIFlagZero);
		default:
			SnekAssert(false);
			return NULL;
		}
	case AST::TypeKind::FloatingPoint:
		switch (type->fpType.precision)
		{
		case FloatingPointPrecision::Half: return LLVMDIBuilderCreateBasicType(module->diBuilder, "half", strlen("half"), 16, 4U, LLVMDIFlagZero);
		case FloatingPointPrecision::Single: return LLVMDIBuilderCreateBasicType(module->diBuilder, "float", strlen("float"), 32, 4U, LLVMDIFlagZero);
		case FloatingPointPrecision::Double: return LLVMDIBuilderCreateBasicType(module->diBuilder, "double", strlen("double"), 64, 4U, LLVMDIFlagZero);
		case FloatingPointPrecision::Quad: return LLVMDIBuilderCreateBasicType(module->diBuilder, "float128", strlen("float128"), 128, 4U, LLVMDIFlagZero);
		default:
			SnekAssert(false);
			return NULL;
		}
	case AST::TypeKind::Boolean:
		return LLVMDIBuilderCreateBasicType(module->diBuilder, "bool", strlen("bool"), 8, 2U, LLVMDIFlagZero);
	case AST::TypeKind::Struct:
	{
		LLVMTypeRef llvmType = (LLVMTypeRef)type->structType.declaration->typeHandle;

		int numFields = type->structType.numFields;
		LLVMMetadataRef* fieldTypes = new LLVMMetadataRef[numFields];
		for (int i = 0; i < numFields; i++)
		{
			LLVMMetadataRef memberTypeInfo = NULL;
			if (type->structType.fieldTypes[i]->typeKind == AST::TypeKind::Pointer || type->structType.fieldTypes[i]->typeKind == AST::TypeKind::Function)
			{
				memberTypeInfo = DebugInfoGetType(llb, module, GetPointerType(GetVoidType()));
			}
			else
			{
				memberTypeInfo = DebugInfoGetType(llb, module, type->structType.fieldTypes[i]);
			}

			uint64_t size = LLVMDITypeGetSizeInBits(memberTypeInfo);
			uint64_t offset = LLVMOffsetOfElement(llb->targetData, llvmType, i) * 8;
			LLVMMetadataRef memberType = LLVMDIBuilderCreateMemberType(
				module->diBuilder, NULL,
				type->structType.fieldNames[i], strlen(type->structType.fieldNames[i]),
				LLVMDIScopeGetFile(module->diCompileUnit),
				type->structType.declaration->fields[i]->location.line,
				size,
				0,
				offset,
				LLVMDIFlagZero,
				memberTypeInfo
			);

			fieldTypes[i] = memberType;
		}

		int size = 0;
		if (numFields > 0)
		{
			size = (int)LLVMSizeOfTypeInBits(llb->targetData, llvmType);
		}

		LLVMMetadataRef structType = LLVMDIBuilderCreateStructType(
			module->diBuilder, module->diCompileUnit,
			type->structType.name, strlen(type->structType.name),
			LLVMDIScopeGetFile(module->diCompileUnit),
			type->structType.declaration->location.line,
			size,
			0,
			LLVMDIFlagZero,
			NULL,
			fieldTypes, numFields,
			0,
			NULL,
			NULL,
			0
		);

		delete[] fieldTypes;

		return structType;
	}
	case AST::TypeKind::Class:
	{
		LLVMTypeRef llvmType = (LLVMTypeRef)type->classType.declaration->typeHandle;

		int numFields = type->classType.numFields;
		LLVMMetadataRef* fieldTypes = new LLVMMetadataRef[numFields];
		for (int i = 0; i < numFields; i++)
		{
			LLVMMetadataRef memberTypeInfo = DebugInfoGetType(llb, module, type->classType.fieldTypes[i]);

			uint64_t size = LLVMDITypeGetSizeInBits(memberTypeInfo);
			uint64_t offset = LLVMOffsetOfElement(llb->targetData, llvmType, i) * 8;
			LLVMMetadataRef memberType = LLVMDIBuilderCreateMemberType(
				module->diBuilder, NULL,
				type->classType.fieldNames[i], strlen(type->classType.fieldNames[i]),
				LLVMDIScopeGetFile(module->diCompileUnit),
				type->classType.declaration->fields[i]->location.line,
				size,
				0,
				offset,
				LLVMDIFlagZero,
				memberTypeInfo
			);
			fieldTypes[i] = memberType;
		}

		int size = 0;
		if (numFields > 0)
		{
			size = (int)LLVMSizeOfTypeInBits(llb->targetData, llvmType);
		}

		LLVMMetadataRef structType = LLVMDIBuilderCreateStructType(
			module->diBuilder, module->diCompileUnit,
			type->classType.name, strlen(type->classType.name),
			LLVMDIScopeGetFile(module->diCompileUnit),
			type->classType.declaration->location.line,
			size,
			0,
			LLVMDIFlagZero,
			NULL,
			fieldTypes, numFields,
			0,
			NULL,
			NULL,
			0
		);
		//LLVMMetadataRef classType = LLVMDIBuilderCreateObjectPointerType(module->diBuilder, structType);
		LLVMMetadataRef classType = LLVMDIBuilderCreatePointerType(module->diBuilder, structType, 64, 0, 0, NULL, 0);

		delete[] fieldTypes;

		return classType;
	}
	case AST::TypeKind::Alias:
	{
		LLVMMetadataRef alias = DebugInfoGetType(llb, module, type->aliasType.alias);
		return LLVMDIBuilderCreateTypedef(module->diBuilder, alias, type->aliasType.name, strlen(type->aliasType.name), LLVMDIScopeGetFile(module->diCompileUnit), type->aliasType.declaration->location.line, module->diCompileUnit, 0);
	}
	case AST::TypeKind::Pointer:
	{
		LLVMMetadataRef elementType = DebugInfoGetType(llb, module, type->pointerType.elementType);
		int size = 64;
		return LLVMDIBuilderCreatePointerType(module->diBuilder, elementType, size, 0, 0, NULL, 0);
	}
	case AST::TypeKind::Function:
	{
		LLVMMetadataRef elementType = NULL;
		int size = 64;
		return LLVMDIBuilderCreatePointerType(module->diBuilder, elementType, 64, 0, 0, NULL, 0);
	}
	case AST::TypeKind::Array:
	{
		LLVMMetadataRef elementType = DebugInfoGetType(llb, module, type->arrayType.elementType);
		return LLVMDIBuilderCreateArrayType(module->diBuilder, type->arrayType.length, 0, elementType, NULL, 0);
	}
	case AST::TypeKind::String:
	{
		return stringType;
	}
	default:
		SnekAssert(false);
		return nullptr;
	}
}

LLVMMetadataRef DebugInfoGetType(LLVMBackend* llb, SkModule* module, TypeID type)
{
	auto result = module->debugTypes.find(type);
	if (result != module->debugTypes.end())
		return result->second;
	else
	{
		LLVMMetadataRef debugType = DebugInfoCreateType(llb, module, type);
		module->debugTypes.emplace(type, debugType);
		return debugType;
	}
}

static LLVMMetadataRef DebugInfoCreateSubprogram(LLVMBackend* llb, SkModule* module, const char* name, const char* linkageName, int line, int scopeLine, int numParams, LLVMMetadataRef* paramTypes)
{
	LLVMMetadataRef file = LLVMDIScopeGetFile(module->diCompileUnit);

	unsigned int filenameLen = 0;
	const char* filename = LLVMDIFileGetFilename(file, &filenameLen);
	unsigned int directoryLen = 0;
	const char* directory = LLVMDIFileGetDirectory(file, &directoryLen);

	LLVMMetadataRef unit = LLVMDIBuilderCreateFile(module->diBuilder, filename, filenameLen, directory, directoryLen);
	LLVMMetadataRef diFuncType = LLVMDIBuilderCreateSubroutineType(module->diBuilder, unit, paramTypes, numParams, LLVMDIFlagZero);

	LLVMMetadataRef functionContext = unit;

	LLVMMetadataRef subProgram = LLVMDIBuilderCreateFunction(module->diBuilder, functionContext, name, strlen(name), linkageName, strlen(linkageName), unit, 0, diFuncType, false, true, scopeLine, LLVMDIFlagZero, false);

	return subProgram;
}

LLVMMetadataRef DebugInfoBeginFunction(LLVMBackend* llb, SkModule* module, AST::Function* function, LLVMValueRef value, TypeID instanceType)
{
	List<LLVMMetadataRef> debugParams = CreateList<LLVMMetadataRef>();
	//if (function->returnType->typeKind != TYPE_KIND_VOID)
	debugParams.add(DebugInfoGetType(llb, module, function->functionType->functionType.returnType));
	if (instanceType)
		debugParams.add(DebugInfoGetType(llb, module, instanceType));
	for (int i = 0; i < function->paramTypes.size; i++)
	{
		debugParams.add(DebugInfoGetType(llb, module, function->paramTypes[i]->typeID));
	}

	LLVMMetadataRef subProgram = DebugInfoCreateSubprogram(llb, module, function->name, function->mangledName, function->location.line, function->body->location.line, debugParams.size, debugParams.buffer);

	LLVMSetSubprogram(value, subProgram);

	DebugInfoPushScope(llb, module, subProgram);

	DestroyList(debugParams);

	return subProgram;
}

void DebugInfoEndFunction(LLVMBackend* llb, SkModule* module, AST::Function* function)
{
	/*
	if (function->body->statementKind == STATEMENT_KIND_COMPOUND)
		DebugInfoEmitSourceLocation(llb, module, module->builder, ((AstCompoundStatement*)function->body)->statements.last()->inputState.line + 1, 0);
	else
		DebugInfoEmitSourceLocation(llb, module, module->builder, function->body->inputState.line + 1, 0);
		*/
	LLVMMetadataRef subProgram = DebugInfoPopScope(llb, module);
}

void DebugInfoDeclareVariable(LLVMBackend* llb, SkModule* module, LLVMValueRef alloc, TypeID type, const char* name, const AST::SourceLocation& location)
{
	LLVMMetadataRef scope = module->debugScopes.back();
	LLVMMetadataRef file = LLVMDIVariableGetScope(scope);
	LLVMMetadataRef debugType = DebugInfoGetType(llb, module, type);
	LLVMMetadataRef varInfo = LLVMDIBuilderCreateAutoVariable(module->diBuilder, scope, name, strlen(name), file, location.line, debugType, true, LLVMDIFlagZero, 0);

	LLVMMetadataRef locationMD = LLVMDIBuilderCreateDebugLocation(llb->llvmContext, location.line, location.col, scope, NULL);
	LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(module->diBuilder, NULL, 0);
	LLVMValueRef function = (LLVMValueRef)module->currentFunction->valueHandle;
	LLVMBasicBlockRef block = LLVMGetInsertBlock(module->builder);

	LLVMDIBuilderInsertDeclareAtEnd(module->diBuilder, alloc, varInfo, expression, locationMD, block);
}

void DebugInfoDeclareParameter(LLVMBackend* llb, SkModule* module, LLVMValueRef alloc, int argIndex, TypeID type, const char* name, const AST::SourceLocation& location)
{
	LLVMMetadataRef scope = module->debugScopes.back();
	SnekAssert(LLVMGetMetadataKind(scope) == LLVMDISubprogramMetadataKind);
	LLVMMetadataRef file = LLVMDIVariableGetScope(scope);
	LLVMMetadataRef debugType = DebugInfoGetType(llb, module, type);
	LLVMMetadataRef varInfo = LLVMDIBuilderCreateParameterVariable(module->diBuilder, scope, name, strlen(name), argIndex + 1, file, location.line, debugType, true, LLVMDIFlagZero);

	LLVMMetadataRef locationMD = LLVMDIBuilderCreateDebugLocation(llb->llvmContext, location.line, location.col, scope, nullptr);
	LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(module->diBuilder, nullptr, 0);
	LLVMValueRef function = (LLVMValueRef)module->currentFunction->valueHandle;
	LLVMBasicBlockRef block = LLVMGetInsertBlock(module->builder);

	LLVMDIBuilderInsertDeclareAtEnd(module->diBuilder, alloc, varInfo, expression, locationMD, block);
}

void DebugInfoDeclareGlobal(LLVMBackend* llb, SkModule* module, LLVMValueRef alloc, const char* name, const char* mangledName, TypeID type, const AST::SourceLocation& location)
{
	LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(module->diBuilder, NULL, 0);
	LLVMMetadataRef global = LLVMDIBuilderCreateGlobalVariableExpression(
		module->diBuilder, module->diCompileUnit,
		name, strlen(name),
		mangledName, strlen(mangledName),
		LLVMDIScopeGetFile(module->diCompileUnit),
		location.line,
		DebugInfoGetType(llb, module, type),
		false,
		expression,
		NULL,
		0
	);
	LLVM_CALL(LLVMGlobalSetMetadata, alloc, LLVMDIGlobalVariableExpressionMetadataKind, global);
}
