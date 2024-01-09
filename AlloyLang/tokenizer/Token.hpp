#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string_view>

namespace AlloyCompiler
{
	using TokenID = size_t;

	/// <summary>
	/// General description of a token.
	/// Each value can apply to multiple tokens.
	/// </summary>
	enum class TokenKind : uint8_t
	{
		Unknown = 0,

		// qualifiers
		Qualifier,

		// declarations
		Declaration,

		// built-in types
		BuiltInType,

		// control flow
		ControlFlow,

		// literals
		Literal,

		// delimiters
		Delimiter,

		// operators
		Operator,

		// assignment
		Assignment,

		// any user defined name
		Identifier,

		// end of file
		EndOfFile,

		MAX_ENUM
	};

	/// <summary>
	/// Specific description of a token.
	/// Each value applies to one token only.
	/// </summary>
	enum class TokenValue : uint8_t
	{
		Unknown = 0,

		// qualifiers
		Exp, Pub,

		// declarations
		Var, Const, Fn, Struct, Enum,

		// built-in types
		Void, I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, Bool,

		// control flow
		If, Else, While, For, Break, Continue, Return,

		// literals
		True, False, Null, Integer, Float, String, Character,

		// delimiters
		OpenParen, CloseParen, OpenBrace, CloseBrace, Comma, Semicolon, Colon,

		// operators
		Plus, Minus, /*alias*/ Negate = Minus, Multiply, /*alias*/ Pointer = Multiply, Divide, Modulo,
		LogicalAnd, LogicalOr, LeftShift, RightShift,
		BitwiseAnd, /*alias*/ Reference = BitwiseAnd, BitwiseOr, BitwiseXor, BitwiseNot,
		Equals, NotEquals, LessThan, GreaterThan, LessThanOrEqual, GreaterThanOrEqual,

		Not,

		Arrow,

		Dereference,

		// assignment operators
		Assign, PlusAssign, MinusAssign, MultiplyAssign, DivideAssign, ModuloAssign,
		BitwiseAndAssign, BitwiseOrAssign, BitwiseXorAssign, BitwiseNotAssign,

		// any user defined name
		Identifier,

		// end of file
		EndOfFile,

		MAX_ENUM
	};

	/// <summary>
	/// Description of a token's kind and value.
	/// </summary>
	struct Token
	{
		TokenKind Kind;
		TokenValue Value;

		constexpr explicit Token(TokenKind kind, TokenValue value)
			: Kind(kind), Value(value)
		{}
	};

	/// <summary>
	/// Description of a token's location in the source code using line and column numbers.
	/// </summary>
	struct Location
	{
		uint32_t LineStart;
		uint32_t Line;
		uint32_t Column;

		constexpr explicit Location(uint32_t lineStart, uint32_t line, uint32_t column)
			: LineStart(lineStart), Line(line), Column(column)
		{}
	};

	/// <summary>
	/// A view into the source code string for tokens with extra data.
	/// </summary>
	using SourceView = std::string_view;

}
