#pragma once

#include <llvm-c/Core.h>


struct LLVMBackend;
struct SkModule;

namespace AST
{
	struct SourceLocation;

	struct Element;
	struct Function;
}

typedef struct TypeData* TypeID;


void InitDebugInfo(LLVMBackend* llb, SkModule* module, const char* filename, const char* directory);
void CompleteDebugInfo(LLVMBackend* llb, SkModule* module);
void DebugInfoEmitSourceLocation(LLVMBackend* llb, SkModule* module, LLVMBuilderRef builder, const AST::SourceLocation& location);
void DebugInfoEmitNullLocation(LLVMBackend* llb, SkModule* module, LLVMBuilderRef builder);
void DebugInfoEmitSourceLocation(LLVMBackend* llb, SkModule* module, AST::Element* element);
void DebugInfoPushScope(LLVMBackend* llb, SkModule* module, LLVMMetadataRef scope);
LLVMMetadataRef DebugInfoPopScope(LLVMBackend* llb, SkModule* module);
LLVMMetadataRef DebugInfoGetType(LLVMBackend* llb, SkModule* module, TypeID type);
LLVMMetadataRef DebugInfoBeginFunction(LLVMBackend* llb, SkModule* module, AST::Function* function, LLVMValueRef value, TypeID instanceType);
void DebugInfoEndFunction(LLVMBackend* llb, SkModule* module, AST::Function* function);
void DebugInfoDeclareVariable(LLVMBackend* llb, SkModule* module, LLVMValueRef alloc, TypeID type, const char* name, const AST::SourceLocation& location);
void DebugInfoDeclareParameter(LLVMBackend* llb, SkModule* module, LLVMValueRef alloc, int argIndex, TypeID type, const char* name, const AST::SourceLocation& location);
void DebugInfoDeclareGlobal(LLVMBackend* llb, SkModule* module, LLVMValueRef alloc, const char* name, const char* mangledName, TypeID type, const AST::SourceLocation& location);
