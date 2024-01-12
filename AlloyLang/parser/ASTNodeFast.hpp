#pragma once
#include <cstdint>

#include "tokenizer/Token.hpp"

namespace AlloyCompiler
{
	using NodeID = uint32_t;

	using NodeVec = std::vector<NodeID>;	// TODO: make smaller implementation


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

	struct Identifier
	{
		enum class Type : uint8_t
		{
			Type,
		};

		union Data
		{
			TypeIdentifier Type;
		};

		Type Type;
		Data Data;
	};

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
		FunctionCall(TokenID nameID, NodeVec arguments)
			: NameID(nameID), Arguments(arguments)
		{}

		TokenID NameID;
		NodeVec Arguments;
	};

	struct Enclosed
	{
		Enclosed(NodeVec tupleExpressions)
			: TupleExpressions(tupleExpressions)
		{}

		NodeVec TupleExpressions;
	};

#pragma endregion

	struct Expression
	{
		enum class Type : uint8_t
		{
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
			Enclosed
		};

		union Data
		{
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
		};

		Type Type;
		Data Data;
	};

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
		StatementBlock(NodeVec statements)
			: Statements(statements)
		{}

		NodeVec Statements;
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
		Match(NodeID expression, NodeVec cases)
			: Expression(expression), Cases(cases)
		{}

		NodeID Expression;
		NodeVec Cases;
	};

#pragma endregion

	struct Statement
	{
		enum class Type : uint8_t
		{
			Return,
			StatementBlock,
			For,
			While,
			If,
			Match
		};

		union Data
		{
			Return Return;
			StatementBlock StatementBlock;
			For For;
			While While;
			If If;
			Match Match;
		};

		Type Type;
		Data Data;
	};

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
		TupleTypeDeclaration(NodeVec types)
			: Types(types)
		{}

		NodeVec Types;
	};

#pragma endregion

	struct TypeDeclaration
	{
		enum class Type : uint8_t
		{
			Var,
			Const,
			Tuple
		};

		union Data
		{
			VariableTypeDeclaration Var;
			ConstantTypeDeclaration Const;
			TupleTypeDeclaration Tuple;
		};

		Type Type;
		Data Data;
	};

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

	struct Declaration
	{
		enum class Type : uint8_t
		{
			Var,
			Const
		};

		union Data
		{
			VariableDeclaration Var;
			ConstantDeclaration Const;
		};

		Type Type;
		Data Data;
	};

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
		StructDefinition(TokenID nameID, NodeVec members)
			: NameID(nameID), Members(members)
		{}

		TokenID NameID;
		NodeVec Members;
	};

	struct EnumDefinition
	{
		EnumDefinition(TokenID nameID, NodeVec members)
			: NameID(nameID), Members(members)
		{}

		TokenID NameID;
		NodeVec Members;
	};

	struct FunctionDefinition
	{
		FunctionDefinition(TokenID nameID, NodeVec parameters, NodeID returnType, NodeID body)
			: NameID(nameID), ReturnType(returnType), Parameters(parameters), Body(body)
		{}

		TokenID NameID;
		NodeID ReturnType;
		NodeVec Parameters;
		NodeID Body;
	};

#pragma endregion

	struct Definition
	{
		enum class Type : uint8_t
		{
			Var,
			Const,
			Struct,
			Enum,
			Function
		};

		union Data
		{
			VariableDefinition Var;
			ConstantDefinition Const;
			StructDefinition Struct;
			EnumDefinition Enum;
			FunctionDefinition Function;
		};

		Type Type;
		Data Data;
	};

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
		Module(NodeVec qualifiedDefinitions)
			: QualifiedDefinitions(qualifiedDefinitions)
		{}

		NodeVec QualifiedDefinitions;
	};

	struct Program
	{
		Program(NodeVec modules)
			: Modules(modules)
		{}

		NodeVec Modules;
	};

	enum class NodeKind : uint8_t
	{
		Identifier,

		Expression,
		Statement,

		TypeDeclaration,
		Declaration,

		Definition,
		QualifiedDefinition,

		Module,
		Program
	};

	struct Node
	{
		NodeKind Kind;

		union
		{
			Identifier Identifier;

			Expression Expression;
			Statement Statement;

			TypeDeclaration TypeDeclaration;
			Declaration Declaration;

			Definition Definition;
			QualifiedDefinition QualifiedDefinition;

			Module Module;
			Program Program;
		};

	};

	void a()
	{
		Node node
		{
			NodeKind::Identifier,
			Identifier
			{
				Identifier::Type::Type,
				TypeIdentifier
				{
					TypeIdentifier::Modifier::None,
					0
				}
			}
		};
	}

	// TODO: need default copyable std::vector<NodeID> implementation


}
