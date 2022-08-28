#pragma once

#include "utils/List.h"
#include "semantics/Type.h"

#include <llvm-c/Core.h>
#include <llvm-c/DebugInfo.h>
#include <llvm-c/TargetMachine.h>

#include <map>
#include <string>
#include <stack>


struct SkContext;

namespace AST
{
	struct File;
	struct Function;
	struct Struct;
	struct Class;
	struct GlobalVariable;
	struct Type;
	struct Statement;
}

struct LLVMBackend
{
	SkContext* context;

	LLVMContextRef llvmContext;
	LLVMTargetMachineRef targetMachine;
	LLVMTargetDataRef targetData;
	char* targetTriple;

	std::map<AST::Struct*, LLVMTypeRef> structTypes;
	std::map<AST::Class*, LLVMTypeRef> classTypes;
};

struct GenericData
{
	std::map<std::string, LLVMTypeRef> types;
};

struct SkModule
{
	AST::File* ast;

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

	std::map<AST::Function*, LLVMValueRef> functionValues;
	std::map<AST::GlobalVariable*, LLVMValueRef> globalValues;

	AST::Function* currentFunction = nullptr;
	AST::Struct* currentStruct = nullptr;

	std::stack<GenericData> genericData;
};


LLVMBackend* CreateLLVMBackend(SkContext* context);
void DestroyLLVMBackend(LLVMBackend* llb);

LLVMTypeRef GenTypeID(LLVMBackend* llb, SkModule* module, TypeID type, AST::Type* ast = nullptr);
LLVMTypeRef GenType(LLVMBackend* llb, SkModule* module, AST::Type* type);
LLVMValueRef GetRValue(LLVMBackend* llb, SkModule* module, LLVMValueRef value, bool lvalue);

bool BlockHasBranched(LLVMBackend* llb, SkModule* module);
void GenStatement(LLVMBackend* llb, SkModule* module, AST::Statement* statement);

LLVMValueRef GenFunctionHeader(LLVMBackend* llb, SkModule* module, AST::Function* decl);
LLVMValueRef GenFunction(LLVMBackend* llb, SkModule* module, AST::Function* decl);

LLVMTypeRef GenStructHeader(LLVMBackend* llb, SkModule* module, AST::Struct* decl);
LLVMTypeRef GenStruct(LLVMBackend* llb, SkModule* module, AST::Struct* decl);

bool LLVMBackendCompile(LLVMBackend* llb, AST::File** asts, int numModules, const char* filename, const char* buildFolder, bool genDebugInfo, bool emitLLVM, int optLevel);
bool LLVMLink(LLVMBackend* llb, const char* arg0, const char* filename, bool genDebugInfo, int optLevel, List<const char*>& additionalLibPaths);
