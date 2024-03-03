#pragma once
#include <cstdint>

#include "../tokenizer/Token.hpp"

namespace AlloyCompiler
{
	enum class NodeID : uint32_t
	{};

	constexpr NodeID ERROR_NODE_ID = (NodeID)std::numeric_limits<uint32_t>::max();

	/// <summary>
	/// This type is required because union members cannot have destructors.
	/// Therefore, we allocate the vector elsewhere and pass a reference to the nodes.
	/// </summary>
	template<typename T>
	using VectorRef = std::vector<T>&;


	struct LITERAL
	{
		enum class Type : uint8_t
		{
			Integer,
			Float,
			Boolean,
			String,
			Character
		};

		Type Kind;
		TokenID InfoTokenID;
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
			Pointer,
		};

		Modifier Mod;
		NodeID ArraySizeID;			// ERROR_NODE_ID if not an array
		NodeID IdentifierOrTypeIdentifierID;	// can be ID of TYPE_IDENTIFIER for multi-dimensional arrays, or IDENTIFIER for basic types
	};

#pragma endregion

#pragma region Declarations

	struct TYPE_DECLARATION
	{
		enum class Type : uint8_t
		{
			Copy,
			Variable,
			Constant
		};

		Type Kind;
		NodeID TypeIdentifierID;
	};

	struct VALUE_DECLARATION
	{
		enum class Type : uint8_t
		{
			Variable,
			Constant
		};

		Type Kind;
		NodeID IdentifierID;
		NodeID TypeIdentifierID;
	};

	struct FUNCTION_DECLARATION
	{
		NodeID IdentifierID;
		VectorRef<NodeID> ParameterIDs;
		NodeID ReturnTypeID;
		bool IsVariadic;
	};

#pragma endregion

#pragma region Expressions

	struct CONSTRUCTOR_EXPRESSION
	{
		NodeID StructIdentifierID;
		VectorRef<NodeID> MemberIdentifierIDs;	// name of the member to assign the corresponding value in MemberValueIDs to
		VectorRef<NodeID> MemberValueIDs;		// value to assign to the corresponding member in MemberIdentifierIDs
	};

	struct POINTER_INITIALIZER_EXPRESSION
	{
		NodeID ValueID;
		NodeID CountID;	// ERROR_NODE_ID if not an array
	};

	struct INITIALIZER_LIST_EXPRESSION
	{
		VectorRef<NodeID> ValueIDs;
	};

	struct VALUE_DEFINITION_EXPRESSION
	{
		NodeID ValueDeclarationID;
		NodeID ValueID;
	};

	struct ARRAY_ACCESS_EXPRESSION
	{
		NodeID ArrayIdentifierID;
		NodeID IndexExpressionID;
	};

	struct MEMBER_ACCESS_EXPRESSION
	{
		NodeID LeftID;
		NodeID RightID;
	};

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

	union PRIMARY_EXPRESSION
	{
		IDENTIFIER Identifier;
		LITERAL Literal;
		CONSTRUCTOR_EXPRESSION ConstructorExpression;
		POINTER_INITIALIZER_EXPRESSION PointerInitializerExpression;
		INITIALIZER_LIST_EXPRESSION InitializerListExpression;
		VALUE_DEFINITION_EXPRESSION ValueDefinitionExpression;
		ARRAY_ACCESS_EXPRESSION ArrayAccessExpression;
		MEMBER_ACCESS_EXPRESSION MemberAccessExpression;
		FUNCTION_CALL_EXPRESSION FunctionCallExpression;
		ENCLOSED_EXPRESSION EnclosedExpression;
	};

	struct ASSIGNMENT_EXPRESSION
	{
		NodeID IdentifierOrMemberAccessID;	// IDENTIFIER or MEMBER_ACCESS_EXPRESSION
		TokenID OperatorTokenID;
		NodeID ValueID;
	};

	union EXPRESSION
	{
		BINARY_EXPRESSION BinaryExpression;
		UNARY_EXPRESSION UnaryExpression;
		PRIMARY_EXPRESSION PrimaryExpression;
		ASSIGNMENT_EXPRESSION AssignmentExpression;
	};

#pragma endregion

#pragma region Statements

	struct VALUE_DEFINITION_STATEMENT
	{
		NodeID ValueDefinitionExpressionID;
	};

	struct FUNCTION_CALL_STATEMENT
	{
		NodeID FunctionCallExpressionID;
	};

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
		NodeID ElseID;
	};

	struct BLOCK_STATEMENT
	{
		VectorRef<NodeID> StatementIDs;
	};

	struct RETURN_STATEMENT
	{
		NodeID ExpressionID;
	};

	union STATEMENT
	{
		VALUE_DEFINITION_STATEMENT ValueDefinitionStatement;
		FUNCTION_CALL_STATEMENT FunctionCallStatement;
		ASSIGNMENT_STATEMENT AssignmentStatement;
		FOR_LOOP_STATEMENT ForLoopStatement;
		WHILE_LOOP_STATEMENT WhileLoopStatement;
		IF_STATEMENT IfStatement;
		BLOCK_STATEMENT BlockStatement;
		RETURN_STATEMENT ReturnStatement;
	};

#pragma endregion

#pragma region Definitions

	struct EXTERN_DEFINITION
	{
		NodeID FunctionDeclarationID;
	};

	struct VALUE_DEFINITION
	{
		NodeID ValueDefinitionExpressionID;
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
		NodeID FunctionDeclarationID;
		NodeID BodyID;
	};

	union DEFINITION
	{
		EXTERN_DEFINITION ExternDefinition;
		VALUE_DEFINITION ValueDefinition;
		STRUCT_DEFINITION StructDefinition;
		ENUM_DEFINITION EnumDefinition;
		FUNCTION_DEFINITION FunctionDefinition;
	};

	struct QUALIFIED_DEFINITION
	{
		enum class Qualifier : uint8_t
		{
			Private,
			Public,
			Export,

			Default = Private
		};

		Qualifier Visibility;
		NodeID DefinitionID;
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
		FUNCTION_DECLARATION,

		CONSTRUCTOR_EXPRESSION,
		POINTER_INITIALIZER_EXPRESSION,
		INITIALIZER_LIST_EXPRESSION,
		VALUE_DEFINITION_EXPRESSION,
		ARRAY_ACCESS_EXPRESSION,
		MEMBER_ACCESS_EXPRESSION,
		FUNCTION_CALL_EXPRESSION,
		ENCLOSED_EXPRESSION,

		BINARY_EXPRESSION,
		UNARY_EXPRESSION,

		//PRIMARY_EXPRESSION,

		ASSIGNMENT_EXPRESSION,

		//EXPRESSION,

		VALUE_DEFINITION_STATEMENT,
		FUNCTION_CALL_STATEMENT,
		ASSIGNMENT_STATEMENT,
		FOR_LOOP_STATEMENT,
		WHILE_LOOP_STATEMENT,
		IF_STATEMENT,
		BLOCK_STATEMENT,
		RETURN_STATEMENT,

		//STATEMENT,

		EXTERN_DEFINITION,
		VALUE_DEFINITION,
		STRUCT_DEFINITION,
		ENUM_DEFINITION,
		FUNCTION_DEFINITION,

		//DEFINITION,
		QUALIFIED_DEFINITION,

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
			FUNCTION_DECLARATION FunctionDeclaration;

			CONSTRUCTOR_EXPRESSION ConstructorExpression;
			POINTER_INITIALIZER_EXPRESSION PointerInitializerExpression;
			INITIALIZER_LIST_EXPRESSION InitializerListExpression;
			VALUE_DEFINITION_EXPRESSION ValueDefinitionExpression;
			ARRAY_ACCESS_EXPRESSION ArrayAccessExpression;
			MEMBER_ACCESS_EXPRESSION MemberAccessExpression;
			FUNCTION_CALL_EXPRESSION FunctionCallExpression;
			ENCLOSED_EXPRESSION EnclosedExpression;

			BINARY_EXPRESSION BinaryExpression;
			UNARY_EXPRESSION UnaryExpression;

			//PRIMARY_EXPRESSION PrimaryExpression;

			ASSIGNMENT_EXPRESSION AssignmentExpression;

			//EXPRESSION Expression;

			VALUE_DEFINITION_STATEMENT ValueDefinitionStatement;
			FUNCTION_CALL_STATEMENT FunctionCallStatement;
			ASSIGNMENT_STATEMENT AssignmentStatement;
			FOR_LOOP_STATEMENT ForLoopStatement;
			WHILE_LOOP_STATEMENT WhileLoopStatement;
			IF_STATEMENT IfStatement;
			BLOCK_STATEMENT BlockStatement;
			RETURN_STATEMENT ReturnStatement;

			//STATEMENT Statement;

			EXTERN_DEFINITION ExternDefinition;
			VALUE_DEFINITION ValueDefinition;
			STRUCT_DEFINITION StructDefinition;
			ENUM_DEFINITION EnumDefinition;
			FUNCTION_DEFINITION FunctionDefinition;

			//DEFINITION Definition;
			QUALIFIED_DEFINITION QualifiedDefinition;

			MODULE Module;
			PROGRAM Program;
		};
	};

	class NodeBuffers
	{
	public:
		NodeID CreateNode(const Node& node, TokenID errorInfo);

		VectorRef<NodeID> CreateNodeIDVector();
		VectorRef<TokenID> CreateTokenIDVector();

		const Node& GetNode(NodeID id) const;
		TokenID GetErrorInfo(NodeID id) const;

		void SetRootNodeID(NodeID id);
		NodeID GetRootNodeID() const;

	private:
		std::vector<Node> m_Nodes;
		std::vector<TokenID> m_ErrorInfos;

		std::vector<std::unique_ptr<std::vector<NodeID>>> m_NodeIDVectors;
		std::vector<std::unique_ptr<std::vector<TokenID>>> m_TokenIDVectors;

		NodeID m_RootNodeID = ERROR_NODE_ID;
	};
}
