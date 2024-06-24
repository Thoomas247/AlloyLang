#pragma once
#include <unordered_map>

#include "Source.hpp"

namespace AlloyCompiler
{
	/// <summary>
	/// TokenKind enum to match SYNTAX.txt.
	/// </summary>
	enum class TokenKind : uint8_t
	{
		none = 0,

		end_of_file,

		identifier,
		long_identifier,

		extern_keyword,
		struct_keyword,
		enum_keyword,
		function_keyword,
		macro_keyword,

		import_keyword,

		public_keyword,
		export_keyword,

		type_keyword,

		variable_keyword,
		constant_keyword,

		for_keyword,
		while_keyword,
		if_keyword,
		else_keyword,
		return_keyword,

		new_keyword,
		move_keyword,

		pound,
		at_symbol,

		reference,
		//pointer,

		comma,
		colon,
		semicolon,
		double_colon,
		dot,
		arrow,
		ellipsis,

		open_paren,
		close_paren,
		open_brace,
		close_brace,
		open_bracket,
		close_bracket,

		assignment_operator,

		unary_operator,
		binary_operator,

		integer_literal,
		float_literal,
		boolean_literal,
		string_literal,
		character_literal
	};

	struct Token
	{
		std::string_view Value;
		Location Location;
		TokenKind Kind;
	};

	/// <summary>
	/// Maps TokenKind enum values to their string representation.
	/// </summary>
	const std::unordered_map<TokenKind, std::string> TOKEN_KIND_VALUES =
	{
		{ TokenKind::none,					"none" },

		{ TokenKind::identifier,			"identifier" },
		{ TokenKind::long_identifier,		"identifier" },

		{ TokenKind::extern_keyword,		"extern"},
		{ TokenKind::struct_keyword,		"struct" },
		{ TokenKind::enum_keyword,			"enum" },
		{ TokenKind::function_keyword,		"fn" },
		{ TokenKind::macro_keyword,			"macro" },

		{ TokenKind::import_keyword,		"import" },

		{ TokenKind::public_keyword,		"pub" },
		{ TokenKind::export_keyword,		"exp" },

		{ TokenKind::type_keyword,			"type" },

		{ TokenKind::pound,					"#" },
		{ TokenKind::at_symbol,				"@" },

		{ TokenKind::variable_keyword,		"var" },
		{ TokenKind::constant_keyword,		"const" },

		{ TokenKind::for_keyword,			"for" },
		{ TokenKind::while_keyword,			"while" },
		{ TokenKind::if_keyword,			"if" },
		{ TokenKind::else_keyword,			"else" },
		{ TokenKind::return_keyword,		"return" },

		{ TokenKind::new_keyword,			"new" },
		{ TokenKind::move_keyword,			"move" },

		{ TokenKind::reference,				"&" },
		//{ TokenKind::pointer,				"*" },

		{ TokenKind::comma,					"," },
		{ TokenKind::colon,					":" },
		{ TokenKind::semicolon,				";" },
		{ TokenKind::double_colon,			"::" },
		{ TokenKind::dot,					"." },
		{ TokenKind::arrow,					"->" },
		{ TokenKind::ellipsis,				"..." },

		{ TokenKind::open_paren,			"(" },
		{ TokenKind::close_paren,			")" },
		{ TokenKind::open_brace,			"{" },
		{ TokenKind::close_brace,			"}" },
		{ TokenKind::open_bracket,			"[" },
		{ TokenKind::close_bracket,			"]" },

		{ TokenKind::assignment_operator,	"=" },

		{ TokenKind::unary_operator,		"unary operator" },
		{ TokenKind::binary_operator,		"binary operator" },

		{ TokenKind::integer_literal,		"integer literal" },
		{ TokenKind::float_literal,			"float literal" },
		{ TokenKind::boolean_literal,		"boolean literal" },
		{ TokenKind::string_literal,		"string literal" },
		{ TokenKind::character_literal,		"character literal" }
	};

	/// <summary>
	/// Maps known symbols to their TokenKind enum values.
	/// Matches SYNTAX.txt.
	/// </summary>
	const std::unordered_map<std::string_view, TokenKind> KNOWN_SYMBOLS =
	{
		{ "extern", TokenKind::extern_keyword },
		{ "struct", TokenKind::struct_keyword },
		{ "enum", TokenKind::enum_keyword },
		{ "fn", TokenKind::function_keyword },
		{ "macro", TokenKind::macro_keyword },

		{ "import", TokenKind::import_keyword },

		{ "pub", TokenKind::public_keyword },
		{ "exp", TokenKind::export_keyword },

		{ "type", TokenKind::type_keyword },

		{ "var", TokenKind::variable_keyword },
		{ "const", TokenKind::constant_keyword },

		{ "for", TokenKind::for_keyword },
		{ "while", TokenKind::while_keyword },
		{ "if", TokenKind::if_keyword },
		{ "else", TokenKind::else_keyword },
		{ "return", TokenKind::return_keyword },

		{ "new", TokenKind::new_keyword },
		{ "move", TokenKind::move_keyword },

		{ "#", TokenKind::pound },
		{ "@", TokenKind::at_symbol },

		{ "&", TokenKind::reference },
		//{ "*", TokenKind::pointer },

		{ ",", TokenKind::comma },
		{ ":", TokenKind::colon },
		{ ";", TokenKind::semicolon },
		{ "::", TokenKind::double_colon },
		{ ".", TokenKind::dot },
		{ "->", TokenKind::arrow },
		{ "...", TokenKind::ellipsis },

		{ "(", TokenKind::open_paren },
		{ ")", TokenKind::close_paren },
		{ "{", TokenKind::open_brace },
		{ "}", TokenKind::close_brace },
		{ "[", TokenKind::open_bracket },
		{ "]", TokenKind::close_bracket },

		{ "=", TokenKind::assignment_operator },

		{ "&&", TokenKind::binary_operator },
		{ "||", TokenKind::binary_operator },
		{ "==", TokenKind::binary_operator },
		{ "!=", TokenKind::binary_operator },
		{ "<", TokenKind::binary_operator },
		{ ">", TokenKind::binary_operator },
		{ "<=", TokenKind::binary_operator },
		{ ">=", TokenKind::binary_operator },
		{ "+", TokenKind::binary_operator },
		{ "-", TokenKind::binary_operator },
		{ "*", TokenKind::binary_operator },
		{ "/", TokenKind::binary_operator },
		{ "%", TokenKind::binary_operator },

		{ "!", TokenKind::unary_operator },
		//{ "-", TokenKind::unary_operator },

		{ "true", TokenKind::boolean_literal },
		{ "false", TokenKind::boolean_literal }
	};

	/// <summary>
	/// Maps the first character of an operator to every possible character that can follow to form the full operator.
	/// </summary>
	const std::unordered_map<char, std::vector<char>> OPERATOR_COMBINATIONS =
	{
		{ '&', { '&' } },	// & or &&
		{ '|', { '|' } },	// | or ||
		{ '=', { '=' } },	// = or ==
		{ '!', { '=' } },	// ! or !=
		{ '<', { '=' } },	// < or <=
		{ '>', { '=' } },	// > or >=
		{ '+', { } },		// +
		{ '-', { '>'}},		// - or ->
		{ '*', { } },		// *
		{ '/', { } },		// /
		{ '%', { } },		// %
		{ '.', { } },		// . (... doesn't fit here as it is 3 characters long, case is handled manually in tokenizer)
		{ ':', { ':' }},	// : or ::
	};

	/// <summary>
	/// Holds the data for all tokens in a source file.
	/// Token data can be accessed by the TokenID returned by AddToken, or through the iterator.
	/// </summary>
	class TokenBuffers
	{
	public:
		/// <summary>
		/// Creates a new token.
		/// </summary>
		void AddToken(TokenKind kind, const std::string_view& value, const Location& location);

		/// <summary>
		/// Returns the token at the given index.
		/// </summary>
		Token* GetToken(size_t index);

		/// <summary>
		/// Returns the number of tokens.
		/// </summary>
		size_t NumTokens() const;

	private:
		std::vector<Token> m_Tokens;
	};
}
