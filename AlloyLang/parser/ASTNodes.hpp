#pragma once
#include <cstdint>

#include "tokenizer/Token.hpp"

namespace AlloyCompiler
{
	enum class NodeID : uint32_t
	{};

	constexpr NodeID ERROR_NODE_ID = std::numeric_limits<NodeID>::max();

	/// <summary>
	/// This type is required because union members cannot have destructors.
	/// Therefore, we allocate the vector elsewhere and pass a reference to the nodes.
	/// </summary>
	template<typename T>
	using VectorRef = std::vector<T>&;


	struct LITERAL
	{
		enum class Kind : uint8_t
		{
			Integer,
			Float,
			Boolean,
			String,
			Character
		};

		Kind Kind;
		TokenID Info;
	};


#pragma region Identifiers

	struct IDENTIFIER
	{
		TokenID IdentifierTokenID;
	};

	struct TYPE_IDENTIFIER
	{
		enum class Modifier : uint8_t
		{
			None,
			Reference,
			Pointer
		};

		Modifier Mod;
		NodeID IdentifierID;
	};

#pragma endregion

#pragma region Declarations

	struct TYPE_DECLARATION
	{
		enum class Kind : uint8_t
		{
			Variable,
			Constant
		};

		Kind Kind;
		NodeID TypeIdentifierID;
	};

	struct VALUE_DECLARATION
	{
		enum class Kind : uint8_t
		{
			Variable,
			Constant
		};

		Kind Kind;
		NodeID IdentifierID;
		NodeID TypeIdentifierID;
	};

#pragma endregion

#pragma region Definitions

	struct VALUE_DEFINITION
	{
		NodeID ValueDeclarationID;
		TokenID OperatorTokenID;
		NodeID ValueID;
	};

	struct STRUCT_DEFINITION
	{
		NodeID IdentifierID;
		VectorRef<NodeID> MemberIDs;
	};

	struct ENUM_DEFINITION
	{
		NodeID IdentifierID;
		VectorRef<NodeID> MemberIDs;
	};

	struct FUNCTION_DEFINITION
	{
		NodeID IdentifierID;
		VectorRef<NodeID> ParameterIDs;
		NodeID ReturnTypeIdentifierID;
		NodeID BodyID;
	};

	struct QUALIFIED_DEFINITION
	{
		enum class Qualifier : uint8_t
		{
			Private,
			Public,
			Export
		};

		Qualifier Visibility;
		NodeID DefinitionID;
	};

#pragma endregion

#pragma region Expressions

	struct FUNCTION_CALL_EXPRESSION
	{
		NodeID IdentifierID;
		VectorRef<NodeID> ArgumentIDs;
	};

	struct ENCLOSED_EXPRESSION
	{
		NodeID ExpressionID;
	};

	struct BINARY_EXPRESSION
	{
		TokenID OperatorTokenID;
		NodeID LeftID;
		NodeID RightID;
	};

	struct UNARY_EXPRESSION
	{
		TokenID OperatorTokenID;
		NodeID OperandID;
	};

	struct ASSIGNMENT_EXPRESSION
	{
		NodeID IdentifierID;
		TokenID OperatorTokenID;
		NodeID ValueID;
	};

#pragma endregion

#pragma region Statements

	struct ASSIGNMENT_STATEMENT
	{
		NodeID AssignmentExpressionID;
	};

	struct FOR_LOOP_STATEMENT
	{
		NodeID InitExpressionID;
		NodeID ConditionExpressionID;
		NodeID IncrementExpressionID;
		NodeID BodyID;
	};

	struct WHILE_LOOP_STATEMENT
	{
		NodeID ConditionExpressionID;
		NodeID BodyID;
	};

	struct IF_STATEMENT
	{
		NodeID ConditionExpressionID;
		NodeID BodyID;
		NodeID ElseBodyID;
	};

	struct BLOCK_STATEMENT
	{
		VectorRef<NodeID> StatementIDs;
	};

	struct RETURN_STATEMENT
	{
		NodeID ExpressionID;
	};

#pragma endregion

	struct MODULE
	{
		VectorRef<NodeID> QualifiedDefinitionIDs;
	};

	struct PROGRAM
	{
		VectorRef<NodeID> ModuleIDs;
	};


	enum class NodeKind : uint8_t
	{
		LITERAL,

		IDENTIFIER,
		TYPE_IDENTIFIER,

		TYPE_DECLARATION,
		VALUE_DECLARATION,

		VALUE_DEFINITION,
		STRUCT_DEFINITION,
		ENUM_DEFINITION,
		FUNCTION_DEFINITION,

		//DEFINITION,
		QUALIFIED_DEFINITION,

		FUNCTION_CALL_EXPRESSION,
		ENCLOSED_EXPRESSION,

		BINARY_EXPRESSION,
		UNARY_EXPRESSION,

		//PRIMARY_EXPRESSION,

		ASSIGNMENT_EXPRESSION,

		//EXPRESSION,

		ASSIGNMENT_STATEMENT,
		FOR_LOOP_STATEMENT,
		WHILE_LOOP_STATEMENT,
		IF_STATEMENT,
		BLOCK_STATEMENT,
		RETURN_STATEMENT,

		//STATEMENT,

		MODULE,
		PROGRAM
	};


	struct Node
	{
		const NodeKind Kind;

		union
		{
			LITERAL Literal;

			IDENTIFIER Identifier;
			TYPE_IDENTIFIER TypeIdentifier;

			TYPE_DECLARATION TypeDeclaration;
			VALUE_DECLARATION ValueDeclaration;

			VALUE_DEFINITION ValueDefinition;
			STRUCT_DEFINITION StructDefinition;
			ENUM_DEFINITION EnumDefinition;
			FUNCTION_DEFINITION FunctionDefinition;

			//DEFINITION Definition;
			QUALIFIED_DEFINITION QualifiedDefinition;

			FUNCTION_CALL_EXPRESSION FunctionCallExpression;
			ENCLOSED_EXPRESSION EnclosedExpression;

			BINARY_EXPRESSION BinaryExpression;
			UNARY_EXPRESSION UnaryExpression;

			//PRIMARY_EXPRESSION PrimaryExpression;

			ASSIGNMENT_EXPRESSION AssignmentExpression;

			//EXPRESSION Expression;

			ASSIGNMENT_STATEMENT AssignmentStatement;
			FOR_LOOP_STATEMENT ForLoopStatement;
			WHILE_LOOP_STATEMENT WhileLoopStatement;
			IF_STATEMENT IfStatement;
			BLOCK_STATEMENT BlockStatement;
			RETURN_STATEMENT ReturnStatement;

			//STATEMENT Statement;

			MODULE Module;
			PROGRAM Program;
		};
	};
}
