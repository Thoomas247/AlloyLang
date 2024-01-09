#pragma once

#include <string>
#include <vector>
#include <memory>

namespace AlloyCompiler
{
	template <typename T>
	using Ptr = std::unique_ptr<T>;

	template <typename T>
	using Vec = std::vector<Ptr<T>>;

	struct ASTNode
	{
		ASTNode() = default;
		virtual ~ASTNode() = default;

		virtual std::string ToString() = 0;
	};

	struct TypeIdentifier : public ASTNode
	{
		enum class Modifier
		{
			None,
			Reference,
			Pointer
		};

		TypeIdentifier(Modifier modifier, const std::string_view& name)
			: Mod(modifier), Name(name)
		{}
		~TypeIdentifier() = default;

		std::string ToString() override
		{
			return "TYPE_IDENTIFIER";
		}


		Modifier Mod;
		std::string_view Name;
	};

#pragma region Expressions

	struct Expression : public ASTNode
	{
		Expression() = default;
		~Expression() = default;

		std::string ToString() override = 0;
	};

#pragma endregion

#pragma region Statements

	struct Statement : public ASTNode
	{
		Statement() = default;
		~Statement() = default;

		std::string ToString() override = 0;
	};

	struct StatementBlock : public Statement
	{
		StatementBlock(Vec<Statement> statements)
			: Statements(std::move(statements))
		{}
		~StatementBlock() = default;

		std::string ToString() override
		{
			return "STATEMENT_BLOCK";
		}


		Vec<Statement> Statements;
	};

#pragma endregion

#pragma region Declarations

	struct TypeDeclaration : public ASTNode
	{
		TypeDeclaration() = default;
		~TypeDeclaration() = default;

		std::string ToString() override = 0;
	};

	struct ValueTypeDeclaration : public TypeDeclaration
	{
		ValueTypeDeclaration(Ptr<TypeIdentifier> type)
			: Type(std::move(type))
		{}
		~ValueTypeDeclaration() = default;

		std::string ToString() override
		{
			return "VALUE_TYPE_DECLARATION";
		}


		Ptr<TypeIdentifier> Type;
	};

	struct VariableTypeDeclaration : public TypeDeclaration
	{
		VariableTypeDeclaration(Ptr<TypeIdentifier> type)
			: Type(std::move(type))
		{}
		~VariableTypeDeclaration() = default;

		std::string ToString() override
		{
			return "VAR_TYPE_DECLARATION";
		}


		Ptr<TypeIdentifier> Type;
	};

	struct ConstantTypeDeclaration : public TypeDeclaration
	{
		ConstantTypeDeclaration(Ptr<TypeIdentifier> type)
			: Type(std::move(type))
		{}
		~ConstantTypeDeclaration() = default;

		std::string ToString() override
		{
			return "CONST_TYPE_DECLARATION";
		}


		Ptr<TypeIdentifier> Type;
	};

	struct TupleTypeDeclaration : public TypeDeclaration
	{
		TupleTypeDeclaration(Vec<TypeDeclaration> typeDeclarations)
			: TypeDeclarations(std::move(typeDeclarations))
		{}
		~TupleTypeDeclaration() = default;

		std::string ToString() override
		{
			return "TUPLE_TYPE_DECLARATION";
		}


		Vec<TypeDeclaration> TypeDeclarations;
	};

	struct Declaration : public ASTNode
	{
		Declaration() = default;
		~Declaration() = default;

		std::string ToString() override = 0;
	};

	struct VariableDeclaration : public Declaration
	{
		VariableDeclaration(const std::string_view& name, Ptr<TypeIdentifier> typeIdentifier)
			: Name(name), Type(std::move(typeIdentifier))
		{}
		~VariableDeclaration() = default;

		std::string ToString() override
		{
			return "VAR_DECLARATION";
		}


		std::string_view Name;
		Ptr<TypeIdentifier> Type;
	};

	struct ConstantDeclaration : public Declaration
	{
		ConstantDeclaration(const std::string_view& name, Ptr<TypeIdentifier> typeIdentifier)
			: Name(name), Type(std::move(typeIdentifier))
		{}
		~ConstantDeclaration() = default;

		std::string ToString() override
		{
			return "CONST_DECLARATION";
		}


		std::string_view Name;
		Ptr<TypeIdentifier> Type;
	};

#pragma endregion

#pragma region Definitions

	struct Definition : public ASTNode
	{
		Definition() = default;
		~Definition() = default;

		std::string ToString() override = 0;
	};

	struct VariableDefinition : public Definition
	{
		VariableDefinition(Ptr<VariableDeclaration> varDeclaration, Ptr<Expression> value)
			: VarDeclaration(std::move(varDeclaration)), Value(std::move(value))
		{
		}
		~VariableDefinition() = default;

		std::string ToString() override
		{
			return "VAR_DEFINITION";
		}

		Ptr<VariableDeclaration> VarDeclaration;
		Ptr<Expression> Value;
	};

	struct ConstantDefinition : public Definition
	{
		ConstantDefinition(Ptr<ConstantDeclaration> constDeclaration, Ptr<Expression> value)
			: ConstDeclaration(std::move(constDeclaration)), Value(std::move(value))
		{
		}
		~ConstantDefinition() = default;

		std::string ToString() override
		{
			return "CONST_DEFINITION";
		}

		Ptr<ConstantDeclaration> ConstDeclaration;
		Ptr<Expression> Value;
	};

	struct StructDefinition : public Definition
	{
		StructDefinition(const std::string_view& name, Vec<Declaration> declarations)
			: Name(name), Declarations(std::move(declarations))
		{}
		~StructDefinition() = default;

		std::string ToString() override
		{
			return "STRUCT_DEFINITION";
		}

		std::string_view Name;
		Vec<Declaration> Declarations;
	};

	struct EnumDefinition : public Definition
	{
		EnumDefinition(const std::string_view& name, std::vector<std::string_view> enumValues)
			: Name(name), EnumValues(std::move(enumValues))
		{}
		~EnumDefinition() = default;

		std::string ToString() override
		{
			return "ENUM_DEFINITION";
		}

		std::string_view Name;
		std::vector<std::string_view> EnumValues;
	};

	struct FunctionDefinition : public Definition
	{
		FunctionDefinition(const std::string_view& name, Vec<Declaration> parameters, Ptr<TypeDeclaration> returnType, Ptr<StatementBlock> body)
			: Name(name), Parameters(std::move(parameters)), ReturnType(std::move(returnType)), Body(std::move(body))
		{}
		~FunctionDefinition() = default;

		std::string ToString() override
		{
			return "FUNCTION_DEFINITION";
		}

		std::string_view Name;
		Vec<Declaration> Parameters;
		Ptr<TypeDeclaration> ReturnType;
		Ptr<StatementBlock> Body;
	};

	struct QualifiedDefinition : public ASTNode
	{
		enum class Qualifier
		{
			Private,
			Public,
			Export
		};

		QualifiedDefinition(Qualifier visibility, Ptr<Definition> definition)
			: Visibility(visibility), Def(std::move(definition))
		{}
		~QualifiedDefinition() = default;

		std::string ToString() override
		{
			return "QUALIFIED_DEFINITION";
		}

		Qualifier Visibility;
		Ptr<Definition> Def;
	};

#pragma endregion

	struct Module : public ASTNode
	{
		Module(Vec<QualifiedDefinition> qualifiedDefinitions)
			: QualifiedDefinitions(std::move(qualifiedDefinitions))
		{}
		~Module() = default;

		std::string ToString() override
		{
			return "MODULE";
		}


		Vec<QualifiedDefinition> QualifiedDefinitions;
	};

	struct Program : public ASTNode
	{
		Program(Vec<Module> modules)
			: Modules(std::move(modules))
		{}
		~Program() = default;

		std::string ToString() override
		{
			return "PROGRAM";
		}

		Vec<Module> Modules;
	};
}