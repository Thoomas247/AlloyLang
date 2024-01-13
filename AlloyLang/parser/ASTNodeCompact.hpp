#pragma once
#include <cstdint>

#include "tokenizer/Token.hpp"
#include <variant>

namespace AlloyCompiler
{
	using Index = uint32_t;
	using NodeID = Index;

	/// <summary>
	/// Stores an index and a size of a list of nodes which is stored in a separate array.
	/// </summary>
	struct NodeIDList
	{
		using Size = uint8_t;

		NodeIDList(Index listStart, Size num)
			: ListStart(listStart), Num(num)
		{}

		Index ListStart;
		Size Num;
	};

#pragma region Identifiers

	struct TypeIdentifier
	{
		enum class Modifier : uint8_t
		{
			None,
			Reference,
			Pointer
		};

		TypeIdentifier(Modifier modifier, TokenID nameID)
			: Mod(modifier), NameID(nameID)
		{}

		Modifier Mod;
		TokenID NameID;
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
		StringLiteral(TokenID valueID)
			: ValueID(valueID)
		{}

		TokenID ValueID;
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

	struct Assignment
	{
		Assignment(TokenValue op, NodeID left, NodeID right)
			: Op(op), Left(left), Right(right)
		{}

		TokenValue Op;
		NodeID Left;
		NodeID Right;
	};

	struct MemoryAccess
	{
		MemoryAccess(TokenID nameID)
			: NameID(nameID)
		{}

		TokenID NameID;
	};

	struct FunctionCall
	{
		FunctionCall(TokenID nameID, NodeIDList arguments)
			: NameID(nameID), Arguments(arguments)
		{}

		TokenID NameID;
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
		VariableDeclaration(TokenID nameID, NodeID type)
			: NameID(nameID), Type(type)
		{}

		TokenID NameID;
		NodeID Type;
	};

	struct ConstantDeclaration
	{
		ConstantDeclaration(TokenID nameID, NodeID type, NodeID expression)
			: NameID(nameID), Type(type), Expression(expression)
		{}

		TokenID NameID;
		NodeID Type;
		NodeID Expression;
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
		StructDefinition(TokenID nameID, NodeIDList members)
			: NameID(nameID), Members(members)
		{}

		TokenID NameID;
		NodeIDList Members;
	};

	struct EnumDefinition
	{
		EnumDefinition(TokenID nameID, NodeIDList members)
			: NameID(nameID), Members(members)
		{}

		TokenID NameID;
		NodeIDList Members;
	};

	struct FunctionDefinition
	{
		FunctionDefinition(TokenID nameID, NodeIDList parameters, NodeID returnType, NodeID body)
			: NameID(nameID), ReturnType(returnType), Parameters(parameters), Body(body)
		{}

		TokenID NameID;
		NodeID ReturnType;
		NodeIDList Parameters;
		NodeID Body;
	};

#pragma endregion

	struct QualifiedDefinition
	{
		enum class Qualifier
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
		TypeIdentifier,

		IntegerLiteral,
		FloatLiteral,
		BooleanLiteral,
		StringLiteral,
		CharacterLiteral,

		Binary,
		Unary,
		Assignment,
		MemoryAccess,
		FunctionCall,
		Enclosed,

		Return,
		StatementBlock,
		For,
		While,
		If,
		Match,

		VariableTypeDeclaration,
		ConstantTypeDeclaration,
		TupleTypeDeclaration,

		VariableDeclaration,
		ConstantDeclaration,

		VariableDefinition,
		ConstantDefinition,
		StructDefinition,
		EnumDefinition,
		FunctionDefinition,

		QualifiedDefinition,

		Module,
		Program
	};

	struct Node
	{
		NodeKind Kind;

		union
		{
			TypeIdentifier TypeIdentifier;

			IntegerLiteral IntegerLiteral;
			FloatLiteral FloatLiteral;
			BooleanLiteral BooleanLiteral;
			StringLiteral StringLiteral;
			CharacterLiteral CharacterLiteral;

			Binary Binary;
			Unary Unary;
			Assignment Assignment;
			MemoryAccess MemoryAccess;
			FunctionCall FunctionCall;
			Enclosed Enclosed;

			Return Return;
			StatementBlock StatementBlock;
			For For;
			While While;
			If If;
			Match Match;

			VariableTypeDeclaration VariableTypeDeclaration;
			ConstantTypeDeclaration ConstantTypeDeclaration;
			TupleTypeDeclaration TupleTypeDeclaration;

			VariableDeclaration VariableDeclaration;
			ConstantDeclaration ConstantDeclaration;

			VariableDefinition VariableDefinition;
			ConstantDefinition ConstantDefinition;
			StructDefinition StructDefinition;
			EnumDefinition EnumDefinition;
			FunctionDefinition FunctionDefinition;

			QualifiedDefinition QualifiedDefinition;

			Module Module;
			Program Program;
		};
	};

	Node a()
	{
		Node node
		{
			NodeKind::TypeIdentifier,
			TypeIdentifier
			{
				TypeIdentifier::Modifier::None,
				0
			}
		};
	}

}
