#pragma once

#include "List.h"
#include "Element.h"
#include "Type.h"
#include "Statement.h"
#include "Expression.h"
#include "Variable.h"

#include "semantics/Type.h"

#include <stdint.h>


namespace AST
{
	enum class DeclarationType : uint8_t
	{
		Null = 0,

		Function,
		Struct,
		Class,
		ClassMethod,
		ClassConstructor,
		Typedef,
		Enum,
		Exprdef,
		Variable,
		Module,
		Namespace,
		Import,
	};

	enum class DeclarationFlags : uint16_t
	{
		None = 0,
		Constant = 1 << 0,
		Extern = 1 << 1,
		DllExport = 1 << 2,
		DllImport = 1 << 3,
		Public = 1 << 4,
		Private = 1 << 5,
		Internal = 1 << 6,
		Packed = 1 << 7,
		Constant = 1 << 8,
	};

	struct Declaration : Element
	{
		DeclarationType type;
		DeclarationFlags flags;

		Visibility visibility = Visibility::Null;


		Declaration(File* file, const SourceLocation& location, DeclarationType type, DeclarationFlags flags);

		virtual Element* copy() override = 0;
	};

	struct Function : Declaration
	{
		char* name;
		Type* returnType;
		List<Type*> paramTypes;
		List<char*> paramNames;
		bool varArgs;
		Statement* body;

		bool isGeneric;
		List<char*> genericParams;

		bool isEntryPoint = false;
		char* mangledName = nullptr;
		TypeID type = nullptr;

		TypeID instanceType = nullptr; // For class methods/constructors

		List<Variable*> paramVariables = {};
		Variable* instanceVariable = nullptr;

		ValueHandle valueHandle = nullptr;


		Function(File* file, const SourceLocation& location, DeclarationFlags flags, char* name, Type* returnType, const List<Type*>& paramTypes, const List<char*>& paramNames, bool varArgs, Statement* body, bool isGeneric, const List<char*>& genericParams);
		virtual ~Function();

		virtual Element* copy() override;
	};

	struct StructField : Element
	{
		Type* type;
		char* name;
		int index;


		StructField(File* file, const SourceLocation& location, Type* type, char* name, int index);
		virtual ~StructField();

		virtual Element* copy() override;
	};

	struct Struct : Declaration
	{
		char* name;
		bool hasBody;
		List<StructField*> fields;

		char* mangledName = nullptr;
		TypeID type = nullptr;

		TypeHandle typeHandle = nullptr;


		Struct(File* file, const SourceLocation& location, DeclarationFlags flags, char* name, bool hasBody, const List<StructField*>& fields);
		virtual ~Struct();

		virtual Element* copy() override;
	};

	struct ClassField : Element
	{
		Type* type;
		char* name;
		int index;


		ClassField(File* file, const SourceLocation& location, Type* type, char* name, int index);
		virtual ~ClassField();

		virtual Element* copy() override;
	};

	struct Class : Declaration
	{
		char* name;
		List<ClassField*> fields;
		List<Function*> methods;
		Function* constructor;

		char* mangledName = nullptr;
		TypeID type = nullptr;

		TypeHandle typeHandle = nullptr;


		Class(File* file, const SourceLocation& location, DeclarationFlags flags, char* name, const List<ClassField*>& fields, const List<Function*>& methods, Function* constructor);
		virtual ~Class();

		virtual Element* copy() override;
	};

	struct Typedef : Declaration
	{
		char* name;
		Type* alias;

		TypeID type = nullptr;


		Typedef(File* file, const SourceLocation& location, DeclarationFlags flags, char* name, Type* alias);
		virtual ~Typedef();

		virtual Element* copy() override;
	};

	struct EnumValue : Element
	{
		char* name;
		Expression* value;

		ValueHandle valueHandle = nullptr;


		EnumValue(File* file, const SourceLocation& location, char* name, Expression* value);
		virtual ~EnumValue();

		virtual Element* copy() override;
	};

	struct Enum : Declaration
	{
		char* name;
		Type* alias;
		List<EnumValue*> values;

		TypeID type = nullptr;


		Enum(File* file, const SourceLocation& location, DeclarationFlags flags, char* name, Type* alias, const List<EnumValue*>& values);
		virtual ~Enum();

		virtual Element* copy() override;
	};

	struct Exprdef : Declaration
	{
		char* name;
		Expression* alias;


		Exprdef(File* file, const SourceLocation& location, DeclarationFlags flags, char* name, Expression* alias);
		virtual ~Exprdef();

		virtual Element* copy() override;
	};

	struct GlobalVariable : Declaration
	{
		Type* type;
		List<VariableDeclarator*> declarators;


		GlobalVariable(File* file, const SourceLocation& location, DeclarationFlags flags, Type* type, List<VariableDeclarator*>& declarators);
		virtual ~GlobalVariable();

		virtual Element* copy() override;
	};

	struct ModuleIdentifier
	{
		List<char*> namespaces;
	};

	struct ModuleDeclaration : Declaration
	{
		ModuleIdentifier identifier;

		struct Module* ns = nullptr;


		ModuleDeclaration(File* file, const SourceLocation& location, DeclarationFlags flags, ModuleIdentifier identifier);
		virtual ~ModuleDeclaration();

		virtual Element* copy() override;
	};

	struct NamespaceDeclaration : Declaration
	{
		char* name;


		NamespaceDeclaration(File* file, const SourceLocation& location, DeclarationFlags flags, char* name);
		virtual ~NamespaceDeclaration();

		virtual Element* copy() override;
	};

	struct Import : Declaration
	{
		List<ModuleIdentifier> imports;


		Import(File* file, const SourceLocation& location, DeclarationFlags flags, const List<ModuleIdentifier>& imports);
		virtual ~Import();

		virtual Element* copy() override;
	};
}
