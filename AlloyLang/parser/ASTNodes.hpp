#pragma once
#include <cstdint>

#include "tokenizer/Token.hpp"

namespace AlloyCompiler
{
	using NodeID = uint32_t;

	constexpr NodeID ERROR_NODE_ID = std::numeric_limits<NodeID>::max();

	struct NodeIDList
	{
		NodeIDList(const std::vector<NodeID>& list)
			: List(list)
		{}

		const std::vector<NodeID>& List;
	};

#pragma region Identifiers

	struct Identifier
	{
		Identifier(TokenID tokenID)
			: Token(tokenID)
		{}

		TokenID Token;
	};

	struct TypeIdentifier
	{
		enum class Modifier : uint8_t
		{
			None,
			Reference,
			Pointer
		};

		TypeIdentifier(Modifier modifier, NodeID identifierID)
			: Mod(modifier), IdentifierID(identifierID)
		{}

		Modifier Mod;
		NodeID IdentifierID;
	};

#pragma endregion

#pragma region Expressions

	struct IntegerLiteral
	{
		IntegerLiteral(uint64_t value)
			: Value(value)
		{}

		uint64_t Value;
	};

	struct FloatLiteral
	{
		FloatLiteral(double value)
			: Value(value)
		{}

		double Value;
	};

	struct BooleanLiteral
	{
		BooleanLiteral(bool value)
			: Value(value)
		{}

		bool Value;
	};

	struct StringLiteral
	{
		StringLiteral(std::string_view value)
			: Value(value)
		{}

		std::string_view Value;
	};

	struct CharacterLiteral
	{
		CharacterLiteral(char value)
			: Value(value)
		{}

		char Value;
	};

	struct Binary
	{
		Binary(TokenValue op, NodeID left, NodeID right)
			: Op(op), Left(left), Right(right)
		{}

		TokenValue Op;
		NodeID Left;
		NodeID Right;
	};

	struct Unary
	{
		Unary(TokenValue op, NodeID operand)
			: Op(op), Operand(operand)
		{}

		TokenValue Op;
		NodeID Operand;
	};

	struct AssignmentExpression
	{
		AssignmentExpression(TokenValue op, NodeID identifierID, NodeID value)
			: Op(op), IdentifierID(identifierID), Value(value)
		{}

		TokenValue Op;
		NodeID IdentifierID;
		NodeID Value;
	};

	struct MemoryAccess
	{
		MemoryAccess(NodeID identifierID)
			: IdentifierID(identifierID)
		{}

		NodeID IdentifierID;
	};

	struct FunctionCall
	{
		FunctionCall(NodeID identifierID, NodeIDList arguments)
			: IdentifierID(identifierID), Arguments(arguments)
		{}

		NodeID IdentifierID;
		NodeIDList Arguments;
	};

	struct Enclosed
	{
		Enclosed(NodeIDList tupleExpressions)
			: TupleExpressions(tupleExpressions)
		{}

		NodeIDList TupleExpressions;
	};

#pragma endregion

#pragma region Statements

	struct AssignmentStatement
	{
		AssignmentStatement(NodeID assignment)
			: Assignment(assignment)
		{}

		NodeID Assignment;
	};

	struct Return
	{
		Return(NodeID expression)
			: Expression(expression)
		{}

		NodeID Expression;
	};

	struct StatementBlock
	{
		StatementBlock(NodeIDList statements)
			: Statements(statements)
		{}

		NodeIDList Statements;
	};

	struct For
	{
		For(NodeID init, NodeID condition, NodeID increment, NodeID body)
			: Init(init), Condition(condition), Increment(increment), Body(body)
		{}

		NodeID Init;
		NodeID Condition;
		NodeID Increment;
		NodeID Body;
	};

	struct While
	{
		While(NodeID condition, NodeID body)
			: Condition(condition), Body(body)
		{}

		NodeID Condition;
		NodeID Body;
	};

	struct If
	{
		If(NodeID condition, NodeID body, NodeID elseBody)
			: Condition(condition), Body(body), ElseBody(elseBody)
		{}

		NodeID Condition;
		NodeID Body;
		NodeID ElseBody;
	};

	struct Match
	{
		Match(NodeID expression, NodeIDList cases)
			: Expression(expression), Cases(cases)
		{}

		NodeID Expression;
		NodeIDList Cases;
	};

#pragma endregion

#pragma region TypeDeclarations

	struct VariableTypeDeclaration
	{
		VariableTypeDeclaration(NodeID type)
			: Type(type)
		{}

		NodeID Type;
	};

	struct ConstantTypeDeclaration
	{
		ConstantTypeDeclaration(NodeID type)
			: Type(type)
		{}

		NodeID Type;
	};

	struct ValueTypeDeclaration
	{
		ValueTypeDeclaration(NodeID type)
			: Type(type)
		{}

		NodeID Type;
	};

	struct TupleTypeDeclaration
	{
		TupleTypeDeclaration(NodeIDList types)
			: Types(types)
		{}

		NodeIDList Types;
	};

#pragma endregion

#pragma region Declarations

	struct VariableDeclaration
	{
		VariableDeclaration(NodeID identifierID, NodeID type)
			: IdentifierID(identifierID), Type(type)
		{}

		NodeID IdentifierID;
		NodeID Type;
	};

	struct ConstantDeclaration
	{
		ConstantDeclaration(NodeID identifierID, NodeID type)
			: IdentifierID(identifierID), Type(type)
		{}

		NodeID IdentifierID;
		NodeID Type;
	};

#pragma endregion

#pragma region Definitions

	struct VariableDefinition
	{
		VariableDefinition(NodeID declaration, NodeID value)
			: Declaration(declaration), Value(value)
		{}

		NodeID Declaration;
		NodeID Value;
	};

	struct ConstantDefinition
	{
		ConstantDefinition(NodeID declaration, NodeID value)
			: Declaration(declaration), Value(value)
		{}

		NodeID Declaration;
		NodeID Value;
	};

	struct StructDefinition
	{
		StructDefinition(NodeID identifierID, NodeIDList members)
			: IdentifierID(identifierID), Members(members)
		{}

		NodeID IdentifierID;
		NodeIDList Members;
	};

	struct EnumMember
	{
		EnumMember(NodeID identifierID)
			: IdentifierID(identifierID)
		{}

		NodeID IdentifierID;
	};

	struct EnumDefinition
	{
		EnumDefinition(NodeID identifierID, NodeIDList members)
			: IdentifierID(identifierID), Members(members)
		{}

		NodeID IdentifierID;
		NodeIDList Members;
	};

	struct FunctionDefinition
	{
		FunctionDefinition(NodeID identifierID, NodeIDList parameters, NodeID returnType, NodeID body)
			: IdentifierID(identifierID), ReturnType(returnType), Parameters(parameters), Body(body)
		{}

		NodeID IdentifierID;
		NodeIDList Parameters;
		NodeID ReturnType;
		NodeID Body;
	};

	struct QualifiedDefinition
	{
		enum class Qualifier : uint8_t
		{
			Private,
			Public,
			Export
		};

		QualifiedDefinition(Qualifier visibility, NodeID definition)
			: Visibility(visibility), Definition(definition)
		{}

		Qualifier Visibility;
		NodeID Definition;
	};


#pragma endregion

	struct Module
	{
		Module(NodeIDList qualifiedDefinitions)
			: QualifiedDefinitions(qualifiedDefinitions)
		{}

		NodeIDList QualifiedDefinitions;
	};

	struct Program
	{
		Program(NodeIDList modules)
			: Modules(modules)
		{}

		NodeIDList Modules;
	};

	enum class NodeKind : uint8_t
	{
		Identifier,
		TypeIdentifier,

		IntegerLiteral,
		FloatLiteral,
		BooleanLiteral,
		StringLiteral,
		CharacterLiteral,

		Binary,
		Unary,
		AssignmentExpression,
		MemoryAccess,
		FunctionCall,
		Enclosed,

		AssignmentStatement,
		Return,
		StatementBlock,
		For,
		While,
		If,
		Match,

		VariableTypeDeclaration,
		ConstantTypeDeclaration,
		ValueTypeDeclaration,
		TupleTypeDeclaration,

		VariableDeclaration,
		ConstantDeclaration,

		VariableDefinition,
		ConstantDefinition,
		StructDefinition,
		EnumMember,
		EnumDefinition,
		FunctionDefinition,

		QualifiedDefinition,

		Module,
		Program
	};

	struct Node
	{
		const NodeKind Kind;

		union
		{
			Identifier Identifier;
			TypeIdentifier TypeIdentifier;

			IntegerLiteral IntegerLiteral;
			FloatLiteral FloatLiteral;
			BooleanLiteral BooleanLiteral;
			StringLiteral StringLiteral;
			CharacterLiteral CharacterLiteral;

			Binary Binary;
			Unary Unary;
			AssignmentExpression AssignmentExpression;
			MemoryAccess MemoryAccess;
			FunctionCall FunctionCall;
			Enclosed Enclosed;

			AssignmentStatement AssignmentStatement;
			Return Return;
			StatementBlock StatementBlock;
			For For;
			While While;
			If If;
			Match Match;

			VariableTypeDeclaration VariableTypeDeclaration;
			ConstantTypeDeclaration ConstantTypeDeclaration;
			ValueTypeDeclaration ValueTypeDeclaration;
			TupleTypeDeclaration TupleTypeDeclaration;

			VariableDeclaration VariableDeclaration;
			ConstantDeclaration ConstantDeclaration;

			VariableDefinition VariableDefinition;
			ConstantDefinition ConstantDefinition;
			StructDefinition StructDefinition;
			EnumMember EnumMember;
			EnumDefinition EnumDefinition;
			FunctionDefinition FunctionDefinition;

			QualifiedDefinition QualifiedDefinition;

			Module Module;
			Program Program;
		};
	};
}
