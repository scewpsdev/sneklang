#pragma once

#include <llvm-c/Core.h>


struct LLVMBackend;
struct SkModule;


LLVMValueRef GetStructMember(LLVMBackend* llb, SkModule* module, LLVMValueRef value, int index, const char* name);
LLVMValueRef GetClassMember(LLVMBackend* llb, SkModule* module, LLVMValueRef value, int index, const char* name);

LLVMTypeRef GetStringType(LLVMBackend* llb);
LLVMValueRef CreateStringLiteral(LLVMBackend* llb, SkModule* module, const char* str);
LLVMValueRef GetStringLength(LLVMBackend* llb, SkModule* module, LLVMValueRef value);
LLVMValueRef GetStringBuffer(LLVMBackend* llb, SkModule* module, LLVMValueRef value);
LLVMValueRef GetStringElement(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index);

LLVMTypeRef GetArrayType(LLVMBackend* llb, LLVMTypeRef elementType, int length);
LLVMValueRef GetArrayLength(LLVMBackend* llb, SkModule* module, LLVMValueRef value);
LLVMValueRef GetArrayBuffer(LLVMBackend* llb, SkModule* module, LLVMValueRef value);
LLVMValueRef GetArrayElement(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMValueRef index);
