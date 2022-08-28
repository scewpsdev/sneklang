#pragma once

#include <llvm-c/Core.h>


struct LLVMBackend;
struct SkModule;


LLVMValueRef GetStructMember(LLVMBackend* llb, SkModule* module, LLVMValueRef value, int index, const char* name);
LLVMValueRef GetClassMember(LLVMBackend* llb, SkModule* module, LLVMValueRef value, int index, const char* name);

LLVMTypeRef GetStringType(LLVMBackend* llb);
LLVMValueRef CreateStringLiteral(LLVMBackend* llb, SkModule* module, const char* str);
LLVMValueRef CreateStringOfSize(LLVMBackend* llb, SkModule* module, LLVMValueRef length, bool malloc);
LLVMValueRef GetStringLength(LLVMBackend* llb, SkModule* module, LLVMValueRef value);
LLVMValueRef GetStringBuffer(LLVMBackend* llb, SkModule* module, LLVMValueRef value);
LLVMValueRef GetStringElementAlloc(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index);
LLVMValueRef GetStringElement(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index);
LLVMValueRef StringCompare(LLVMBackend* llb, SkModule* module, LLVMValueRef left, LLVMValueRef right);

LLVMTypeRef GetArrayType(LLVMBackend* llb, LLVMTypeRef elementType, int length);
LLVMValueRef CreateArrayOfSize(LLVMBackend* llb, SkModule* module, LLVMTypeRef elementType, LLVMValueRef size, bool malloc);
LLVMValueRef GetArraySize(LLVMBackend* llb, SkModule* module, LLVMValueRef value);
LLVMValueRef GetArrayBuffer(LLVMBackend* llb, SkModule* module, LLVMValueRef value);
LLVMValueRef GetArrayElementAlloc(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index);
LLVMValueRef GetArrayElement(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index);

LLVMTypeRef GetSliceType(LLVMBackend* llb, LLVMTypeRef elementType);
LLVMValueRef CreateSliceOfSize(LLVMBackend* llb, SkModule* module, LLVMTypeRef elementType, LLVMValueRef size);
LLVMValueRef GetSliceSize(LLVMBackend* llb, SkModule* module, LLVMValueRef value);
LLVMValueRef GetSliceBuffer(LLVMBackend* llb, SkModule* module, LLVMValueRef value);
LLVMValueRef GetSliceElementAlloc(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index);
LLVMValueRef GetSliceElement(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index);
