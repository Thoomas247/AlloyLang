#pragma once
#include "Token.hpp"

namespace AlloyCompiler
{
	/// <summary>
	/// Map of tokens that can be instantly known.
	/// </summary>
	const std::unordered_map<std::string_view, Token> KEYWORDS =
	{
		// qualifiers
		{ "exp",		Token(TokenKind::Qualifier, TokenValue::Exp) },
		{ "pub",		Token(TokenKind::Qualifier, TokenValue::Pub) },

		// declarations
		{ "var",		Token(TokenKind::Declaration, TokenValue::Var) },
		{ "const",		Token(TokenKind::Declaration, TokenValue::Const) },
		{ "fn",			Token(TokenKind::Declaration, TokenValue::Fn) },
		{ "struct",		Token(TokenKind::Declaration, TokenValue::Struct) },
		{ "enum",		Token(TokenKind::Declaration, TokenValue::Enum) },

		// built-in types
		{ "void",		Token(TokenKind::BuiltInType, TokenValue::Void) },
		{ "i8",			Token(TokenKind::BuiltInType, TokenValue::I8) },
		{ "i16",		Token(TokenKind::BuiltInType, TokenValue::I16) },
		{ "i32",		Token(TokenKind::BuiltInType, TokenValue::I32) },
		{ "i64",		Token(TokenKind::BuiltInType, TokenValue::I64) },
		{ "u8",			Token(TokenKind::BuiltInType, TokenValue::U8) },
		{ "u16",		Token(TokenKind::BuiltInType, TokenValue::U16) },
		{ "u32",		Token(TokenKind::BuiltInType, TokenValue::U32) },
		{ "u64",		Token(TokenKind::BuiltInType, TokenValue::U64) },
		{ "f32",		Token(TokenKind::BuiltInType, TokenValue::F32) },
		{ "f64",		Token(TokenKind::BuiltInType, TokenValue::F64) },
		{ "bool",		Token(TokenKind::BuiltInType, TokenValue::Bool) },

		// control flow
		{ "if",			Token(TokenKind::ControlFlow, TokenValue::If) },
		{ "else",		Token(TokenKind::ControlFlow, TokenValue::Else) },
		{ "while",		Token(TokenKind::ControlFlow, TokenValue::While) },
		{ "for",		Token(TokenKind::ControlFlow, TokenValue::For) },
		{ "break",		Token(TokenKind::ControlFlow, TokenValue::Break) },
		{ "continue",	Token(TokenKind::ControlFlow, TokenValue::Continue) },
		{ "return",		Token(TokenKind::ControlFlow, TokenValue::Return) },

		// bool literals
		{ "true",		Token(TokenKind::Literal, TokenValue::True) },
		{ "false",		Token(TokenKind::Literal, TokenValue::False) },
	};

	/// <summary>
	/// Map of every operator to its corresponding token.
	/// </summary>
	const std::unordered_map<std::string_view, Token> OPERATORS =
	{
		{"+",	Token(TokenKind::Operator, TokenValue::Plus)},
		{"-",	Token(TokenKind::Operator, TokenValue::Minus)},
		{"*",	Token(TokenKind::Operator, TokenValue::Multiply)},
		{"/",	Token(TokenKind::Operator, TokenValue::Divide)},
		{"%",	Token(TokenKind::Operator, TokenValue::Modulo)},

		{"&&",	Token(TokenKind::Operator, TokenValue::LogicalAnd)},
		{"||",	Token(TokenKind::Operator, TokenValue::LogicalOr)},
		{"<<",	Token(TokenKind::Operator, TokenValue::LeftShift)},
		{">>",	Token(TokenKind::Operator, TokenValue::RightShift)},

		{"&",	Token(TokenKind::Operator, TokenValue::BitwiseAnd)},
		{"|",	Token(TokenKind::Operator, TokenValue::BitwiseOr)},
		{"^",	Token(TokenKind::Operator, TokenValue::BitwiseXor)},
		{"~",	Token(TokenKind::Operator, TokenValue::BitwiseNot)},

		{"==",	Token(TokenKind::Operator, TokenValue::Equals)},
		{"!=",	Token(TokenKind::Operator, TokenValue::NotEquals)},
		{"<",	Token(TokenKind::Operator, TokenValue::LessThan)},
		{">",	Token(TokenKind::Operator, TokenValue::GreaterThan)},
		{"<=",	Token(TokenKind::Operator, TokenValue::LessThanOrEqual)},
		{">=",	Token(TokenKind::Operator, TokenValue::GreaterThanOrEqual)},


		{"!",	Token(TokenKind::Operator, TokenValue::Not)},
		//{"-",	Token(TokenKind::Operator, TokenValue::Negate)},

		{"->",	Token(TokenKind::Operator, TokenValue::Arrow)},

		//{"*",	Token(TokenKind::Operator, TokenValue::Pointer)},
		//{"&",	Token(TokenKind::Operator, TokenValue::Reference)},
		{"@",	Token(TokenKind::Operator, TokenValue::Dereference)},


		{"=",	Token(TokenKind::Assignment, TokenValue::Assign)},
		{"+=",	Token(TokenKind::Assignment, TokenValue::PlusAssign)},
		{"-=",	Token(TokenKind::Assignment, TokenValue::MinusAssign)},
		{"*=",	Token(TokenKind::Assignment, TokenValue::MultiplyAssign)},
		{"/=",	Token(TokenKind::Assignment, TokenValue::DivideAssign)},
		{"%=",	Token(TokenKind::Assignment, TokenValue::ModuloAssign)},

		{"&=",	Token(TokenKind::Assignment, TokenValue::BitwiseAndAssign)},
		{"|=",	Token(TokenKind::Assignment, TokenValue::BitwiseOrAssign)},
		{"^=",	Token(TokenKind::Assignment, TokenValue::BitwiseXorAssign)},
		{"~=",	Token(TokenKind::Assignment, TokenValue::BitwiseNotAssign)},
	};

	/// <summary>
	/// Map of the first character to every possible character that can follow 
	/// it to form the full operator.
	/// </summary>
	const std::unordered_map<char, std::vector<char>> OPERATOR_COMBINATIONS =
	{
		{'+', {'='}},		// + or +=
		{'-', {'=', '>'}},	// -, -= or ->
		{'*', {'='}},		// * or *=
		{'/', {'='}},		// / or /=

		{'%', {'='}},		// % or %=

		{'&', {'&', '='}},	// &, && or &=
		{'|', {'|', '='}},	// |, || or |=

		{'<', {'<', '='}},	// <, << or <<=
		{'>', {'>', '='}},	// >, >> or >>=

		{'=', {'='}},		// = or ==
		{'!', {'='}},		// ! or !=

		{'@', {}},			// @
	};

	/// <summary>
	/// Map of delimiters to their corresponding token.
	/// </summary>
	const std::unordered_map<char, Token> DELIMITERS =
	{
		{'(',	Token(TokenKind::Delimiter, TokenValue::OpenParen)},
		{')',	Token(TokenKind::Delimiter, TokenValue::CloseParen)},
		{'{',	Token(TokenKind::Delimiter, TokenValue::OpenBrace)},
		{'}',	Token(TokenKind::Delimiter, TokenValue::CloseBrace)},
		{',',	Token(TokenKind::Delimiter, TokenValue::Comma)},
		{';',	Token(TokenKind::Delimiter, TokenValue::Semicolon)},
		{':',	Token(TokenKind::Delimiter, TokenValue::Colon)},
	};
}