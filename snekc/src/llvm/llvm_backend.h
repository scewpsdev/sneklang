#pragma once

#include "list.h"
#include "type.h"

#include <llvm-c/Core.h>
#include <llvm-c/DebugInfo.h>
#include <llvm-c/TargetMachine.h>

#include <map>


struct SkContext;
struct AstFile;
struct AstFunction;
struct AstStruct;
struct AstClass;
struct AstGlobal;

struct LLVMBackend
{
	SkContext* context;

	LLVMContextRef llvmContext;
	LLVMTargetMachineRef targetMachine;
	LLVMTargetDataRef targetData;
	char* targetTriple;

	std::map<AstStruct*, LLVMTypeRef> structTypes;
	std::map<AstClass*, LLVMTypeRef> classTypes;
};

struct SkModule
{
	AstFile* ast;

	bool hasDebugInfo;

	LLVMModuleRef llvmModule;
	LLVMBuilderRef builder;
	LLVMBuilderRef entryBuilder;

	LLVMDIBuilderRef diBuilder;
	LLVMMetadataRef diCompileUnit;
	List<LLVMMetadataRef> debugScopes;
	std::map<TypeID, LLVMMetadataRef> debugTypes;

	LLVMBasicBlockRef returnBlock;
	LLVMValueRef returnAlloc;

	std::map<AstFunction*, LLVMValueRef> functionValues;
	std::map<AstGlobal*, LLVMValueRef> globalValues;

	AstFunction* currentFunction = NULL;
};


LLVMTypeRef CanPassByValue(LLVMBackend* llb, SkModule* module, LLVMTypeRef type);
LLVMValueRef GetRValue(LLVMBackend* llb, SkModule* module, LLVMValueRef value, bool lvalue);

LLVMBackend* CreateLLVMBackend(SkContext* context);
void DestroyLLVMBackend(LLVMBackend* llb);

bool LLVMBackendCompile(LLVMBackend* llb, AstFile** asts, int numModules, const char* filename, const char* buildFolder, bool genDebugInfo, bool emitLLVM, int optLevel);
bool LLVMLink(LLVMBackend* llb, const char* arg0, const char* filename, bool genDebugInfo, int optLevel, List<const char*>& additionalLibPaths);
