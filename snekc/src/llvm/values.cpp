#include "values.h"

#include "LLVMBackend.h"
#include "debug.h"
#include "Types.h"
#include "utils/Log.h"

#include "ast/File.h"

#include "semantics/Variable.h"


LLVMValueRef CastInt(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMTypeRef type, TypeID valueType)
{
	SnekAssert(LLVMGetTypeKind(type) == LLVMIntegerTypeKind);

	LLVMTypeRef vt = LLVMTypeOf(value);

	if (LLVMGetIntTypeWidth(vt) < LLVMGetIntTypeWidth(type))
	{
		if (valueType->integerType.isSigned)
			return LLVM_CALL(LLVMBuildSExt, module->builder, value, type, "");
		else
			return LLVM_CALL(LLVMBuildZExt, module->builder, value, type, "");
	}
	else if (LLVMGetIntTypeWidth(vt) > LLVMGetIntTypeWidth(type))
		return LLVM_CALL(LLVMBuildTrunc, module->builder, value, type, "");
	else
		return value;
}

LLVMValueRef CastValue(LLVMBackend* llb, SkModule* module, LLVMValueRef llvmValue, LLVMTypeRef llvmType, TypeID valueType, TypeID dstType, bool isLValue)
{
	LLVMTypeRef vt = LLVMTypeOf(llvmValue);
	LLVMTypeKind vtk = LLVMGetTypeKind(vt);
	LLVMTypeKind tk = LLVMGetTypeKind(llvmType);

	while (valueType->typeKind == AST::TypeKind::Alias)
		valueType = valueType->aliasType.alias;
	while (dstType->typeKind == AST::TypeKind::Alias)
		dstType = dstType->aliasType.alias;

	if (valueType->typeKind == AST::TypeKind::Integer)
	{
		llvmValue = GetRValue(llb, module, llvmValue, isLValue);

		if (dstType->typeKind == AST::TypeKind::Integer)
		{
			if (valueType->integerType.bitWidth > dstType->integerType.bitWidth)
			{
				return LLVM_CALL(LLVMBuildTrunc, module->builder, llvmValue, llvmType, "");
			}
			else if (valueType->integerType.bitWidth < dstType->integerType.bitWidth)
			{
				if (valueType->integerType.isSigned)
					return LLVM_CALL(LLVMBuildSExt, module->builder, llvmValue, llvmType, "");
				else
					return LLVM_CALL(LLVMBuildZExt, module->builder, llvmValue, llvmType, "");
			}
			else
			{
				return llvmValue;
			}
		}
		else if (dstType->typeKind == AST::TypeKind::FloatingPoint)
		{
			if (valueType->integerType.isSigned)
				return LLVM_CALL(LLVMBuildSIToFP, module->builder, llvmValue, llvmType, "");
			else
				return LLVM_CALL(LLVMBuildUIToFP, module->builder, llvmValue, llvmType, "");
		}
		else if (dstType->typeKind == AST::TypeKind::Boolean)
		{
			return LLVM_CALL(LLVMBuildTrunc, module->builder, llvmValue, llvmType, "");
		}
		else if (dstType->typeKind == AST::TypeKind::Pointer)
		{
			return LLVM_CALL(LLVMBuildIntToPtr, module->builder, llvmValue, llvmType, "");
		}
	}
	else if (valueType->typeKind == AST::TypeKind::FloatingPoint)
	{
		llvmValue = GetRValue(llb, module, llvmValue, isLValue);

		if (dstType->typeKind == AST::TypeKind::FloatingPoint)
		{
			if (valueType->fpType.precision > dstType->fpType.precision)
			{
				return LLVM_CALL(LLVMBuildFPTrunc, module->builder, llvmValue, llvmType, "");
			}
			else if (valueType->fpType.precision < dstType->fpType.precision)
			{
				return LLVM_CALL(LLVMBuildFPExt, module->builder, llvmValue, llvmType, "");
			}
		}
		else if (dstType->typeKind == AST::TypeKind::Integer)
		{
			if (dstType->integerType.isSigned)
				return LLVM_CALL(LLVMBuildFPToSI, module->builder, llvmValue, llvmType, "");
			else
				return LLVM_CALL(LLVMBuildFPToUI, module->builder, llvmValue, llvmType, "");
		}
	}
	else if (valueType->typeKind == AST::TypeKind::Boolean)
	{
		llvmValue = GetRValue(llb, module, llvmValue, isLValue);

		if (dstType->typeKind == AST::TypeKind::Integer)
		{
			if (dstType->integerType.isSigned)
				return LLVM_CALL(LLVMBuildSExt, module->builder, llvmValue, llvmType, "");
			else
				return LLVM_CALL(LLVMBuildZExt, module->builder, llvmValue, llvmType, "");
		}
	}
	else if (valueType->typeKind == AST::TypeKind::Struct)
	{
		llvmValue = GetRValue(llb, module, llvmValue, isLValue);
	}
	else if (valueType->typeKind == AST::TypeKind::Pointer)
	{
		llvmValue = GetRValue(llb, module, llvmValue, isLValue);

		if (dstType->typeKind == AST::TypeKind::Pointer)
		{
			return LLVM_CALL(LLVMBuildBitCast, module->builder, llvmValue, llvmType, "");
		}
		else if (dstType->typeKind == AST::TypeKind::Class)
		{
			if (valueType->pointerType.elementType->typeKind == AST::TypeKind::Void)
			{
				return LLVM_CALL(LLVMBuildBitCast, module->builder, llvmValue, llvmType, "");
			}
		}
		else if (dstType->typeKind == AST::TypeKind::Function)
		{
			if (valueType->pointerType.elementType->typeKind == AST::TypeKind::Void)
			{
				return LLVM_CALL(LLVMBuildBitCast, module->builder, llvmValue, llvmType, "");
			}
		}
		else if (dstType->typeKind == AST::TypeKind::Integer)
		{
			return LLVM_CALL(LLVMBuildPtrToInt, module->builder, llvmValue, llvmType, "");
		}
		/*
		else if (dstType->typeKind == AST::TypeKind::String)
		{
			return LLVM_CALL(LLVMBuildBitCast, module->builder, llvmValue, llvmType, "");
		}
		*/
	}
	else if (valueType->typeKind == AST::TypeKind::Array)
	{
		if (dstType->typeKind == AST::TypeKind::Array)
		{
			if (valueType->arrayType.length != -1 && dstType->arrayType.length == -1)
			{
				LLVMTypeRef arrayType = LLVMGetElementType(LLVMTypeOf(llvmValue));
				LLVMTypeRef elementType = LLVMGetElementType(arrayType);

				if (!CompareTypes(valueType->arrayType.elementType, dstType->arrayType.elementType))
				{
					SnekAssert(valueType->arrayType.length != -1);
					if (dstType->arrayType.length != -1)
						SnekAssert(valueType->arrayType.length == dstType->arrayType.length);

					LLVMTypeRef newElementType = GenTypeID(llb, module, dstType->arrayType.elementType);
					LLVMValueRef castedArray = CreateArrayOfSize(llb, module, newElementType, LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), valueType->arrayType.length, false), false);

					for (int i = 0; i < valueType->arrayType.length; i++)
					{
						LLVMValueRef index = LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), i, false);
						LLVMValueRef elementAlloc = GetArrayElementAlloc(llb, module, castedArray, index);
						LLVMValueRef previousElement = GetArrayElement(llb, module, llvmValue, index);
						LLVMValueRef newElement = CastValue(llb, module, previousElement, newElementType, valueType->arrayType.elementType, dstType->arrayType.elementType, false);
						LLVMBuildStore(module->builder, newElement, elementAlloc);
					}

					llvmValue = castedArray;
					arrayType = GetArrayType(llb, newElementType, valueType->arrayType.length);
					elementType = newElementType;
				}

				SnekAssert(LLVMGetTypeKind(LLVMTypeOf(llvmValue)) == LLVMPointerTypeKind);

				unsigned int arrayLength = LLVMGetArrayLength(arrayType);

				LLVMValueRef sliceValue = LLVMBuildAlloca(module->builder, GetSliceType(llb, elementType), "");
				LLVMValueRef sizeAlloc = LLVMBuildStructGEP(module->builder, sliceValue, 0, "");
				LLVMValueRef ptrAlloc = LLVMBuildStructGEP(module->builder, sliceValue, 1, "");

				LLVMValueRef indices[] = {
					LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 0, false),
					LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 0, false),
				};
				LLVMValueRef ptr = LLVMBuildGEP(module->builder, llvmValue, indices, 2, "");

				LLVMBuildStore(module->builder, LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), arrayLength, false), sizeAlloc);
				LLVMBuildStore(module->builder, ptr, ptrAlloc);

				return sliceValue;
			}
			else
			{
				llvmValue = GetRValue(llb, module, llvmValue, isLValue);
			}

			return llvmValue;
		}
		else
		{
			SnekAssert(false);
		}
	}
	else if (valueType->typeKind == AST::TypeKind::Function)
	{
		llvmValue = GetRValue(llb, module, llvmValue, isLValue);

		if (dstType->typeKind == AST::TypeKind::Function)
		{
			return LLVM_CALL(LLVMBuildBitCast, module->builder, llvmValue, llvmType, "");
		}
		else if (dstType->typeKind == AST::TypeKind::Pointer)
		{
			return LLVM_CALL(LLVMBuildBitCast, module->builder, llvmValue, llvmType, "");
		}
	}
	else if (valueType->typeKind == AST::TypeKind::String)
	{
		llvmValue = GetRValue(llb, module, llvmValue, isLValue);

		/*
		if (dstType->typeKind == AST::TypeKind::Pointer)
		{
			return LLVM_CALL(LLVMBuildBitCast, module->builder, llvmValue, llvmType, "");
		}
		*/
	}

	if (CompareTypes(valueType, dstType))
		return llvmValue;

	SnekAssert(false);
	return nullptr;
}

LLVMValueRef ConstCastValue(LLVMBackend* llb, SkModule* module, LLVMValueRef llvmValue, LLVMTypeRef llvmType, TypeID valueType, TypeID dstType)
{
	SnekAssert(LLVMIsConstant(llvmValue));

	LLVMTypeRef vt = LLVMTypeOf(llvmValue);
	LLVMTypeKind vtk = LLVMGetTypeKind(vt);
	LLVMTypeKind tk = LLVMGetTypeKind(llvmType);

	while (valueType->typeKind == AST::TypeKind::Alias)
		valueType = valueType->aliasType.alias;
	while (dstType->typeKind == AST::TypeKind::Alias)
		dstType = dstType->aliasType.alias;

	if (valueType->typeKind == AST::TypeKind::Integer)
	{
		if (dstType->typeKind == AST::TypeKind::Integer)
		{
			if (valueType->integerType.bitWidth > dstType->integerType.bitWidth)
			{
				return LLVM_CALL(LLVMConstTrunc, llvmValue, llvmType);
			}
			else if (valueType->integerType.bitWidth < dstType->integerType.bitWidth)
			{
				if (valueType->integerType.isSigned)
					return LLVM_CALL(LLVMConstSExt, llvmValue, llvmType);
				else
					return LLVM_CALL(LLVMConstZExt, llvmValue, llvmType);
			}
			else
			{
				return llvmValue;
			}
		}
		else if (dstType->typeKind == AST::TypeKind::FloatingPoint)
		{
			if (valueType->integerType.isSigned)
				return LLVM_CALL(LLVMConstSIToFP, llvmValue, llvmType);
			else
				return LLVM_CALL(LLVMConstUIToFP, llvmValue, llvmType);
		}
		else if (dstType->typeKind == AST::TypeKind::Boolean)
		{
			return LLVM_CALL(LLVMConstTrunc, llvmValue, llvmType);
		}
		else if (dstType->typeKind == AST::TypeKind::Pointer)
		{
			return LLVM_CALL(LLVMConstIntToPtr, llvmValue, llvmType);
		}
	}
	else if (valueType->typeKind == AST::TypeKind::FloatingPoint)
	{
		if (dstType->typeKind == AST::TypeKind::FloatingPoint)
		{
			if (valueType->fpType.precision > dstType->fpType.precision)
			{
				return LLVM_CALL(LLVMConstFPTrunc, llvmValue, llvmType);
			}
			else if (valueType->fpType.precision < dstType->fpType.precision)
			{
				return LLVM_CALL(LLVMConstFPExt, llvmValue, llvmType);
			}
		}
		else if (dstType->typeKind == AST::TypeKind::Integer)
		{
			if (dstType->integerType.isSigned)
				return LLVM_CALL(LLVMConstFPToSI, llvmValue, llvmType);
			else
				return LLVM_CALL(LLVMConstFPToUI, llvmValue, llvmType);
		}
	}
	else if (valueType->typeKind == AST::TypeKind::Boolean)
	{
		if (dstType->typeKind == AST::TypeKind::Integer)
		{
			if (dstType->integerType.isSigned)
				return LLVM_CALL(LLVMConstSExt, llvmValue, llvmType);
			else
				return LLVM_CALL(LLVMConstZExt, llvmValue, llvmType);
		}
	}
	else if (valueType->typeKind == AST::TypeKind::Pointer)
	{
		if (dstType->typeKind == AST::TypeKind::Pointer)
		{
			return LLVM_CALL(LLVMConstBitCast, llvmValue, llvmType);
		}
		else if (dstType->typeKind == AST::TypeKind::Class)
		{
			if (valueType->pointerType.elementType->typeKind == AST::TypeKind::Void)
			{
				return LLVM_CALL(LLVMConstBitCast, llvmValue, llvmType);
			}
		}
		else if (dstType->typeKind == AST::TypeKind::Function)
		{
			if (valueType->pointerType.elementType->typeKind == AST::TypeKind::Void)
			{
				return LLVM_CALL(LLVMConstBitCast, llvmValue, llvmType);
			}
		}
		else if (dstType->typeKind == AST::TypeKind::Integer)
		{
			return LLVM_CALL(LLVMConstPtrToInt, llvmValue, llvmType);
		}
		else if (dstType->typeKind == AST::TypeKind::String)
		{
			return LLVM_CALL(LLVMConstBitCast, llvmValue, llvmType);
		}
	}
	else if (valueType->typeKind == AST::TypeKind::Function)
	{
		if (dstType->typeKind == AST::TypeKind::Function)
		{
			return LLVM_CALL(LLVMConstBitCast, llvmValue, llvmType);
		}
		else if (dstType->typeKind == AST::TypeKind::Pointer)
		{
			return LLVM_CALL(LLVMConstBitCast, llvmValue, llvmType);
		}
	}
	else if (valueType->typeKind == AST::TypeKind::String)
	{
		if (dstType->typeKind == AST::TypeKind::Pointer)
		{
			return LLVM_CALL(LLVMConstBitCast, llvmValue, llvmType);
		}
	}

	if (CompareTypes(valueType, dstType))
		return llvmValue;

	SnekAssert(false);
	return NULL;
}

LLVMValueRef ConvertValue(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMTypeRef type, TypeID valueType, TypeID dstType, bool isLValue, bool isConstant)
{
	//if (isConstant)
	//{
	return CastValue(llb, module, value, type, valueType, dstType, isLValue);
	//}
	/*
	if (CompareTypes(valueType, dstType))
		return value;
	else
	{
		SnekAssert(false);
		return NULL;
	}
	*/
}

LLVMValueRef BitcastValue(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMTypeRef type)
{
	SnekAssert(LLVMSizeOfTypeInBits(llb->targetData, LLVMTypeOf(value)) == LLVMSizeOfTypeInBits(llb->targetData, type));
	LLVMValueRef alloc = AllocateLocalVariable(llb, module, type, "");
	LLVMValueRef bitcasted = LLVMBuildBitCast(module->builder, alloc, LLVMPointerType(LLVMTypeOf(value), 0), "");
	LLVMBuildStore(module->builder, value, bitcasted);
	return LLVMBuildLoad(module->builder, alloc, "");
}

LLVMValueRef AllocateLocalVariable(LLVMBackend* llb, SkModule* module, LLVMTypeRef type, const char* name)
{
	LLVMValueRef currentFunction = module->functionValues[module->currentFunction];
	LLVMBasicBlockRef entryBlock = LLVM_CALL(LLVMGetEntryBasicBlock, currentFunction);
	LLVMValueRef firstInst = LLVM_CALL(LLVMGetFirstInstruction, entryBlock);
	if (firstInst)
	{
		while (LLVMGetInstructionOpcode(firstInst) == LLVMAlloca)
		{
			if (LLVMValueRef nextInst = LLVMGetNextInstruction(firstInst))
			{
				firstInst = nextInst;
			}
			else
			{
				break;
			}
		}
		LLVM_CALL(LLVMPositionBuilder, module->entryBuilder, entryBlock, firstInst);
		//LLVM_CALL(LLVMPositionBuilderAtEnd, module->entryBuilder, firstInst);
	}
	else
		LLVM_CALL(LLVMPositionBuilderAtEnd, module->entryBuilder, entryBlock);

	LLVMValueRef alloc = LLVM_CALL(LLVMBuildAlloca, module->entryBuilder, type, name);

	return alloc;
}
