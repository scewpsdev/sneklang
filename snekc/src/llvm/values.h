#pragma once

#include <llvm-c/Core.h>


struct LLVMBackend;
struct SkModule;
struct Variable;

typedef struct TypeData* TypeID;


LLVMValueRef CastInt(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMTypeRef type, TypeID valueType);
LLVMValueRef CastValue(LLVMBackend* llb, SkModule* module, LLVMValueRef llvmValue, LLVMTypeRef llvmType, TypeID valueType, TypeID dstType, bool isLValue);
LLVMValueRef ConstCastValue(LLVMBackend* llb, SkModule* module, LLVMValueRef llvmValue, LLVMTypeRef llvmType, TypeID valueType, TypeID dstType);
LLVMValueRef ConvertValue(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMTypeRef type, TypeID valueType, TypeID dstType, bool isLValue, bool isConstant);

LLVMValueRef BitcastValue(LLVMBackend* llb, SkModule* module, LLVMValueRef value, LLVMTypeRef type);

LLVMValueRef AllocateLocalVariable(LLVMBackend* llb, SkModule* module, LLVMTypeRef type, const char* name);
