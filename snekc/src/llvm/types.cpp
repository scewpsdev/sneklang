#include "types.h"

#include "llvm_backend.h"
#include "debug.h"
#include "log.h"

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
	return LLVMPointerType(LLVMInt8TypeInContext(llb->llvmContext), 0);

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
	LLVMValueRef array = LLVMConstStringInContext(llb->llvmContext, str, len, false);
	LLVMValueRef alloc = LLVMAddGlobal(module->llvmModule, LLVMTypeOf(array), "");
	LLVMSetInitializer(alloc, array);
	LLVMSetGlobalConstant(alloc, true);
	LLVMSetLinkage(alloc, LLVMPrivateLinkage);

	return LLVMConstBitCast(alloc, LLVMPointerType(LLVMInt8TypeInContext(llb->llvmContext), 0));

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
	LLVMValueRef buffer = LLVM_CALL(LLVMBuildGEP, module->builder, value, (LLVMValueRef*)indices, 2, "");

	return buffer;
}

LLVMValueRef GetStringElement(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index)
{
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind);

	/*
	LLVMValueRef indices[] = {
		LLVM_CALL(LLVMConstInt, LLVMInt32TypeInContext(llb->llvmContext), 0, false),
		LLVM_CALL(LLVMConstInt, LLVMInt32TypeInContext(llb->llvmContext), 1, false),
		index
	};
	LLVMValueRef element = LLVM_CALL(LLVMBuildGEP, module->builder, value, (LLVMValueRef*)indices, 3, "");

	return element;
	*/

	return LLVM_CALL(LLVMBuildGEP, module->builder, value, (LLVMValueRef*)&index, 1, "");
}

LLVMTypeRef GetArrayType(LLVMBackend* llb, LLVMTypeRef elementType, int length)
{
	if (length != -1)
	{
		return LLVM_CALL(LLVMArrayType, elementType, length);
	}
	else
	{
		return elementType;
	}
}

LLVMValueRef GetArrayLength(LLVMBackend* llb, SkModule* module, LLVMValueRef value)
{
	SnekAssert(false); // deprecated

	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind);

	LLVMValueRef lengthPtr = LLVM_CALL(LLVMBuildStructGEP, module->builder, value, 0, "");
	LLVMValueRef length = LLVM_CALL(LLVMBuildLoad, module->builder, lengthPtr, "");

	return length;
}

LLVMValueRef GetArrayBuffer(LLVMBackend* llb, SkModule* module, LLVMValueRef value)
{
	SnekAssert(false); // deprecated

	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind);

	LLVMValueRef indices[] = {
		LLVM_CALL(LLVMConstInt, LLVMInt32TypeInContext(llb->llvmContext), 0, false),
		LLVM_CALL(LLVMConstInt, LLVMInt32TypeInContext(llb->llvmContext), 1, false),
		LLVM_CALL(LLVMConstInt, LLVMInt32TypeInContext(llb->llvmContext), 0, false),
	};
	LLVMValueRef buffer = LLVM_CALL(LLVMBuildGEP, module->builder, value, (LLVMValueRef*)indices, 3, "");

	return buffer;
}

LLVMValueRef GetArrayElement(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index)
{
	LLVMValueRef indices[2] = {
			LLVMConstInt(LLVMTypeOf(index), 0, false),
			index
	};
	return LLVM_CALL(LLVMBuildGEP, module->builder, value, (LLVMValueRef*)indices, 2, "");

	/*
	SnekAssert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind);

	LLVMValueRef indices[] = {
		LLVM_CALL(LLVMConstInt, LLVMInt32TypeInContext(llb->llvmContext), 0, false),
		LLVM_CALL(LLVMConstInt, LLVMInt32TypeInContext(llb->llvmContext), 1, false),
		index
	};
	LLVMValueRef element = LLVM_CALL(LLVMBuildGEP, module->builder, value, (LLVMValueRef*)indices, 3, "");

	return element;
	*/
}
