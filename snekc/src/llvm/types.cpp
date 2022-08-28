#include "types.h"

#include "LLVMBackend.h"
#include "Debug.h"
#include "Values.h"
#include "utils/Log.h"
#include "ast/Declaration.h"

#include <string.h>


LLVMValueRef GetStructMember(LLVMBackend* llb, SkModule* module, LLVMValueRef value, int index, const char* name)
{
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind);

	LLVMValueRef member = LLVM_CALL(LLVMBuildStructGEP, module->builder, value, index, name);

	return member;
}

LLVMValueRef GetClassMember(LLVMBackend* llb, SkModule* module, LLVMValueRef value, int index, const char* name)
{
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(value))) == LLVMPointerTypeKind);

	value = LLVM_CALL(LLVMBuildLoad, module->builder, value, name);
	LLVMValueRef member = LLVM_CALL(LLVMBuildStructGEP, module->builder, value, index, "");

	return member;
}

LLVMTypeRef GetStringType(LLVMBackend* llb)
{
	LLVMTypeRef memberTypes[2] = {
		LLVMInt32TypeInContext(llb->llvmContext),
		LLVMPointerType(LLVMInt8TypeInContext(llb->llvmContext), 0)
	};
	return LLVM_CALL(LLVMStructTypeInContext, llb->llvmContext, memberTypes, 2, false);

	/*
	if (length != -1)
	{
		LLVMTypeRef memberTypes[2] = {
			LLVMInt32TypeInContext(llb->llvmContext),
			LLVMArrayType(LLVMInt8TypeInContext(llb->llvmContext), length)
		};
		return LLVM_CALL(LLVMStructTypeInContext, llb->llvmContext, memberTypes, 2, false);
	}
	else
	{
		LLVMTypeRef memberTypes[2] = {
			LLVMInt32TypeInContext(llb->llvmContext),
			LLVMArrayType(LLVMInt8TypeInContext(llb->llvmContext), 0)
		};
		return LLVM_CALL(LLVMStructTypeInContext, llb->llvmContext, memberTypes, 2, false);
	}

	return LLVMPointerType(LLVMInt8TypeInContext(llb->llvmContext), 0);
	*/

	/*
	LLVMTypeRef elementTypes[2] = {
		LLVM_CALL(LLVMInt32TypeInContext, llb->llvmContext),
		LLVM_CALL(LLVMInt8TypeInContext, llb->llvmContext)
	};

	LLVMTypeRef structType = LLVM_CALL(LLVMStructTypeInContext, llb->llvmContext, (LLVMTypeRef*)elementTypes, 2, false);
	LLVMTypeRef stringType = LLVM_CALL(LLVMPointerType, structType, 0);

	return stringType;
	*/
}

LLVMValueRef CreateStringLiteral(LLVMBackend* llb, SkModule* module, const char* str)
{
	int len = (int)strlen(str);

	LLVMValueRef value = LLVMBuildGlobalStringPtr(module->builder, str, "");

	LLVMValueRef values[2] = {
		LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), len, false),
		value
	};
	LLVMValueRef string = LLVMConstStructInContext(llb->llvmContext, values, 2, false);
	LLVMValueRef alloc = LLVMAddGlobal(module->llvmModule, LLVMTypeOf(string), "");
	LLVMSetInitializer(alloc, string);
	LLVMSetGlobalConstant(alloc, true);
	LLVMSetLinkage(alloc, LLVMPrivateLinkage);

	return alloc; // LLVMConstBitCast(alloc, LLVMPointerType(LLVMInt8TypeInContext(llb->llvmContext), 0));

	/*
	int len = (int)strlen(str);
	//LLVMValueRef array = LLVMConstStringInContext(llb->llvmContext, str, len, false);
	LLVMValueRef values[2] = {
		LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), len, false),
		LLVMConstStringInContext(llb->llvmContext, str, len, false)
	};
	LLVMValueRef string = LLVMConstStructInContext(llb->llvmContext, values, 2, false);
	LLVMValueRef alloc = LLVMAddGlobal(module->llvmModule, LLVMTypeOf(string), "");
	LLVMSetInitializer(alloc, string);
	LLVMSetGlobalConstant(alloc, true);
	LLVMSetLinkage(alloc, LLVMPrivateLinkage);

	return alloc; // LLVMConstBitCast(alloc, LLVMPointerType(LLVMInt8TypeInContext(llb->llvmContext), 0));
	*/

	/*
	if (module->currentFunction)
	{
		return LLVMBuildGlobalStringPtr(module->builder, str, "");
		//return LLVMBuildBitCast(module->builder, array, type, "");
	}
	else
	{


		LLVMValueRef index = LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 0, false);
		bool b = LLVMIsConstant(index);
		return LLVMConstGEP(array, &index, 1);
		//LLVMValueRef alloc = LLVMAddGlobal(module->llvmModule, LLVMTypeOf(array), "");
		//return LLVMConstBitCast(alloc, LLVMPointerType(LLVMInt8TypeInContext(llb->llvmContext), 0));
	}
	*/

	/*
	int len = (int)strlen(str);

	LLVMValueRef values[2] = {
		LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), len, false),
		LLVMConstStringInContext(llb->llvmContext, str, len, false)
	};

	LLVMValueRef value = LLVM_CALL(LLVMConstStructInContext, llb->llvmContext, (LLVMValueRef*)values, 2, false);
	LLVMValueRef alloc = LLVM_CALL(LLVMAddGlobal, module->llvmModule, LLVMTypeOf(value), "");

	LLVM_CALL(LLVMSetGlobalConstant, alloc, true);
	LLVM_CALL(LLVMSetLinkage, alloc, LLVMPrivateLinkage);
	LLVM_CALL(LLVMSetInitializer, alloc, value);

	LLVMTypeRef stringType = GetStringType(llb);

	return LLVM_CALL(LLVMConstBitCast, alloc, stringType);
	*/
}

LLVMValueRef CreateStringOfSize(LLVMBackend* llb, SkModule* module, LLVMValueRef length, bool malloc)
{
	LLVMValueRef alloc = LLVMBuildAlloca(module->builder, GetStringType(llb), "");

	LLVMValueRef buffer = nullptr;
	if (malloc)
		buffer = LLVMBuildArrayMalloc(module->builder, LLVMInt8TypeInContext(llb->llvmContext), length, "");
	else
		buffer = LLVMBuildArrayAlloca(module->builder, LLVMInt8TypeInContext(llb->llvmContext), length, "");

	LLVMValueRef lengthAlloc = LLVMBuildStructGEP(module->builder, alloc, 0, "");
	LLVMValueRef bufferAlloc = LLVMBuildStructGEP(module->builder, alloc, 1, "");

	LLVMBuildStore(module->builder, length, lengthAlloc);
	LLVMBuildStore(module->builder, buffer, bufferAlloc);

	return alloc;
}

LLVMValueRef GetStringLength(LLVMBackend* llb, SkModule* module, LLVMValueRef value)
{
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind);

	LLVMValueRef lengthPtr = LLVM_CALL(LLVMBuildStructGEP, module->builder, value, 0, "");
	LLVMValueRef length = LLVM_CALL(LLVMBuildLoad, module->builder, lengthPtr, "");

	return length;
}

LLVMValueRef GetStringBuffer(LLVMBackend* llb, SkModule* module, LLVMValueRef value)
{
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind);

	LLVMValueRef indices[] = {
		LLVM_CALL(LLVMConstInt, LLVMInt32TypeInContext(llb->llvmContext), 0, false),
		LLVM_CALL(LLVMConstInt, LLVMInt32TypeInContext(llb->llvmContext), 1, false),
		//LLVM_CALL(LLVMConstInt, LLVMInt32TypeInContext(llb->llvmContext), 0, false),
	};
	LLVMValueRef bufferRef = LLVM_CALL(LLVMBuildGEP, module->builder, value, (LLVMValueRef*)indices, 2, "");
	LLVMValueRef buffer = LLVM_CALL(LLVMBuildLoad, module->builder, bufferRef, "");
	LLVMValueRef ptr = LLVMBuildBitCast(module->builder, buffer, LLVMPointerType(LLVMInt8TypeInContext(llb->llvmContext), 0), "");

	return ptr;
}

LLVMValueRef GetStringElementAlloc(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index)
{
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind);

	LLVMValueRef buffer = GetStringBuffer(llb, module, value);
	LLVMValueRef element = LLVM_CALL(LLVMBuildGEP, module->builder, buffer, &index, 1, "");

	return element;

	/*
	LLVMValueRef indices[] = {
		LLVM_CALL(LLVMConstInt, LLVMInt32TypeInContext(llb->llvmContext), 0, false),
		LLVM_CALL(LLVMConstInt, LLVMInt32TypeInContext(llb->llvmContext), 1, false),
		index
	};
	LLVMValueRef element = LLVM_CALL(LLVMBuildGEP, module->builder, value, (LLVMValueRef*)indices, 3, "");

	return element;
	*/

	//return LLVM_CALL(LLVMBuildGEP, module->builder, value, (LLVMValueRef*)&index, 1, "");
}

LLVMValueRef GetStringElement(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index)
{
	LLVMValueRef elementAlloc = GetStringElementAlloc(llb, module, value, index);
	return LLVMBuildLoad(module->builder, elementAlloc, "");
}

LLVMValueRef StringCompare(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right)
{
	LLVMValueRef leftLength = GetStringLength(llb, module, left);
	LLVMValueRef rightLength = GetStringLength(llb, module, right);

	LLVMValueRef resultAlloc = AllocateLocalVariable(llb, module, LLVMInt1TypeInContext(llb->llvmContext), "strcmp_result");
	LLVMBuildStore(module->builder, LLVMConstInt(LLVMInt1TypeInContext(llb->llvmContext), 1, false), resultAlloc);

	LLVMValueRef iteratorAlloc = AllocateLocalVariable(llb, module, LLVMInt32TypeInContext(llb->llvmContext), "strcmp_iterator");
	LLVMBuildStore(module->builder, LLVMConstNull(LLVMInt32TypeInContext(llb->llvmContext)), iteratorAlloc);

	LLVMValueRef llvmFunction = module->functionValues[module->currentFunction];
	LLVMBasicBlockRef loopHeaderBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "strcmp_loop_header");
	LLVMBasicBlockRef loopBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "strcmp_loop");
	LLVMBasicBlockRef loopTailBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "strcmp_loop_tail");
	LLVMBasicBlockRef unequalBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "strcmp_unequal");
	LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlockInContext(llb->llvmContext, llvmFunction, "strcmp_merge");

	LLVMValueRef lengthCmp = LLVMBuildICmp(module->builder, LLVMIntEQ, leftLength, rightLength, "");
	LLVMBuildCondBr(module->builder, lengthCmp, loopHeaderBlock, unequalBlock);

	{
		LLVMPositionBuilderAtEnd(module->builder, loopHeaderBlock);
		LLVMValueRef currentItValue = LLVMBuildLoad(module->builder, iteratorAlloc, "");
		LLVMValueRef itCmp = LLVMBuildICmp(module->builder, LLVMIntULT, currentItValue, leftLength, "");
		LLVMBuildCondBr(module->builder, itCmp, loopBlock, mergeBlock);
	}

	{
		LLVMPositionBuilderAtEnd(module->builder, loopBlock);
		LLVMValueRef currentItValue = LLVMBuildLoad(module->builder, iteratorAlloc, "");
		LLVMValueRef leftChar = GetStringElement(llb, module, left, currentItValue);
		LLVMValueRef rightChar = GetStringElement(llb, module, right, currentItValue);
		LLVMValueRef charCmp = LLVMBuildICmp(module->builder, LLVMIntEQ, leftChar, rightChar, "");
		LLVMBuildCondBr(module->builder, charCmp, loopTailBlock, unequalBlock);
	}

	{
		LLVMPositionBuilderAtEnd(module->builder, loopTailBlock);
		LLVMValueRef currentItValue = LLVMBuildLoad(module->builder, iteratorAlloc, "");
		LLVMValueRef newItValue = LLVMBuildAdd(module->builder, currentItValue, LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 1, false), "");
		LLVMBuildStore(module->builder, newItValue, iteratorAlloc);
		LLVMBuildBr(module->builder, loopHeaderBlock);
	}

	{
		LLVMPositionBuilderAtEnd(module->builder, unequalBlock);
		LLVMBuildStore(module->builder, LLVMConstNull(LLVMInt1TypeInContext(llb->llvmContext)), resultAlloc);
		LLVMBuildBr(module->builder, mergeBlock);
	}

	LLVMPositionBuilderAtEnd(module->builder, mergeBlock);
	LLVMValueRef result = LLVMBuildLoad(module->builder, resultAlloc, "");

	return result;
}

LLVMTypeRef GetArrayType(LLVMBackend* llb, LLVMTypeRef elementType, int length)
{
	SnekAssert(length != 0);

	return LLVMArrayType(elementType, length);
}

LLVMValueRef CreateArrayOfSize(LLVMBackend* llb, SkModule* module, LLVMTypeRef elementType, LLVMValueRef size, bool malloc)
{
	SnekAssert(LLVMIsConstant(size));

	LLVMTypeRef arrayType = GetArrayType(llb, elementType, (int)LLVMConstIntGetZExtValue(size));

	LLVMValueRef alloc = nullptr;
	if (malloc)
		alloc = LLVMBuildMalloc(module->builder, arrayType, "");
	else
		alloc = LLVMBuildAlloca(module->builder, arrayType, "");

	return alloc;
}

LLVMValueRef GetArraySize(LLVMBackend* llb, SkModule* module, LLVMValueRef value)
{
	LLVMTypeRef arrayType = LLVMGetElementType(LLVMTypeOf(value));
	unsigned int arraySize = LLVMGetArrayLength(arrayType);

	return LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), arraySize, false);
}

LLVMValueRef GetArrayBuffer(LLVMBackend* llb, SkModule* module, LLVMValueRef value)
{
	LLVMValueRef indices[] = {
		LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 0, false),
		LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 0, false),
	};
	LLVMValueRef buffer = LLVMBuildGEP(module->builder, value, indices, 2, "");

	return buffer;
}

LLVMValueRef GetArrayElementAlloc(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index)
{
	LLVMValueRef indices[] = {
		LLVMConstInt(LLVMInt32TypeInContext(llb->llvmContext), 0, false),
		index,
	};
	LLVMValueRef element = LLVMBuildGEP(module->builder, value, indices, 2, "");

	return element;
}

LLVMValueRef GetArrayElement(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index)
{
	LLVMValueRef elementAlloc = GetArrayElementAlloc(llb, module, value, index);
	return LLVMBuildLoad(module->builder, elementAlloc, "");
}

LLVMTypeRef GetSliceType(LLVMBackend* llb, LLVMTypeRef elementType)
{
	LLVMTypeRef memberTypes[2] = {
		LLVMInt32TypeInContext(llb->llvmContext),
		LLVMPointerType(elementType, 0)
	};
	return LLVM_CALL(LLVMStructTypeInContext, llb->llvmContext, memberTypes, 2, false);
}

LLVMValueRef CreateSliceOfSize(LLVMBackend* llb, SkModule* module, LLVMTypeRef elementType, LLVMValueRef size)
{
	LLVMValueRef array = LLVMBuildArrayMalloc(module->builder, elementType, size, "");

	LLVMValueRef alloc = LLVMBuildAlloca(module->builder, GetSliceType(llb, elementType), "");
	LLVMValueRef sizeAlloc = LLVMBuildStructGEP(module->builder, alloc, 0, "");
	LLVMValueRef ptrAlloc = LLVMBuildStructGEP(module->builder, alloc, 1, "");

	LLVMBuildStore(module->builder, size, sizeAlloc);
	LLVMBuildStore(module->builder, array, ptrAlloc);

	return alloc;
}

LLVMValueRef GetSliceSize(LLVMBackend* llb, SkModule* module, LLVMValueRef value)
{
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind);

	LLVMValueRef lengthPtr = LLVM_CALL(LLVMBuildStructGEP, module->builder, value, 0, "");
	LLVMValueRef length = LLVM_CALL(LLVMBuildLoad, module->builder, lengthPtr, "");

	return length;
}

LLVMValueRef GetSliceBuffer(LLVMBackend* llb, SkModule* module, LLVMValueRef value)
{
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind);

	LLVMValueRef bufferAlloc = LLVMBuildStructGEP(module->builder, value, 1, "");
	LLVMValueRef buffer = LLVMBuildLoad(module->builder, bufferAlloc, "");

	return buffer;
}

LLVMValueRef GetSliceElementAlloc(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index)
{
	LLVMValueRef buffer = GetSliceBuffer(llb, module, value);
	LLVMValueRef element = LLVMBuildGEP(module->builder, buffer, &index, 1, "");

	return element;
}

LLVMValueRef GetSliceElement(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index)
{
	LLVMValueRef elementAlloc = GetSliceElementAlloc(llb, module, value, index);
	return LLVMBuildLoad(module->builder, elementAlloc, "");
}
