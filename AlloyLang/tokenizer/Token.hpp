#pragma once
#include <unordered_map>

#include "Source.hpp"

namespace AlloyCompiler
{
	/// <summary>
	/// Unique ID for a token. All token data is accessed through this ID.
	/// </summary>
	using TokenID = uint32_t;

	/// <summary>
	/// Represents an invalid token ID.
	/// </summary>
	constexpr TokenID ERROR_TOKEN_ID = (TokenID)std::numeric_limits<uint32_t>::max();

	/// <summary>
	/// TokenKind enum to match SYNTAX.txt.
	/// </summary>
	enum class TokenKind : uint8_t
	{
		none = 0,

		end_of_file,

		identifier,

		export_label,
		public_label,

		extern_keyword,
		struct_keyword,
		enum_keyword,
		function_keyword,

		type_keyword,

		variable_keyword,
		constant_keyword,

		resource_keyword,
		component_keyword,
		query_keyword,
		system_keyword,
		group_keyword,
		application_keyword,

		require_keyword,
		exclude_keyword,

		for_keyword,
		while_keyword,
		if_keyword,
		else_keyword,
		return_keyword,

		new_keyword,
		move_keyword,

		reference,
		//pointer,

		comma,
		colon,
		semicolon,
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

		logical_operator,
		relational_operator,
		additive_operator,
		multiplicative_operator,

		unary_operator,

		integer_literal,
		float_literal,
		boolean_literal,
		string_literal,
		character_literal
	};

	/// <summary>
	/// Maps TokenKind enum values to their string representation.
	/// </summary>
	const std::unordered_map<TokenKind, std::string> TOKEN_KIND_NAMES =
	{
		{ TokenKind::none, "none" },

		{ TokenKind::identifier, "identifier" },

		{ TokenKind::export_label, "export_label" },
		{ TokenKind::public_label, "public_label" },

		{ TokenKind::extern_keyword, "extern_keyword"},
		{ TokenKind::struct_keyword, "struct_keyword" },
		{ TokenKind::enum_keyword, "enum_keyword" },
		{ TokenKind::function_keyword, "function_keyword" },

		{ TokenKind::type_keyword, "type_keyword" },

		{ TokenKind::variable_keyword, "variable_keyword" },
		{ TokenKind::constant_keyword, "constant_keyword" },

		{ TokenKind::resource_keyword, "resource_keyword" },
		{ TokenKind::component_keyword, "component_keyword" },
		{ TokenKind::query_keyword, "query_keyword" },
		{ TokenKind::system_keyword, "system_keyword" },
		{ TokenKind::group_keyword, "group_keyword" },
		{ TokenKind::application_keyword, "application_keyword" },

		{ TokenKind::require_keyword, "require_keyword" },
		{ TokenKind::exclude_keyword, "exclude_keyword" },

		{ TokenKind::for_keyword, "for_keyword" },
		{ TokenKind::while_keyword, "while_keyword" },
		{ TokenKind::if_keyword, "if_keyword" },
		{ TokenKind::else_keyword, "else_keyword" },
		{ TokenKind::return_keyword, "return_keyword" },

		{ TokenKind::new_keyword, "new_keyword" },
		{ TokenKind::move_keyword, "move_keyword" },

		{ TokenKind::reference, "reference" },
		//{ TokenKind::pointer, "pointer" },

		{ TokenKind::comma, "comma" },
		{ TokenKind::colon, "colon" },
		{ TokenKind::semicolon, "semicolon" },
		{ TokenKind::dot, "dot" },
		{ TokenKind::arrow, "arrow" },
		{ TokenKind::ellipsis, "ellipsis" },

		{ TokenKind::open_paren, "open_paren" },
		{ TokenKind::close_paren, "close_paren" },
		{ TokenKind::open_brace, "open_brace" },
		{ TokenKind::close_brace, "close_brace" },
		{ TokenKind::open_bracket, "open_bracket" },
		{ TokenKind::close_bracket, "close_bracket" },

		{ TokenKind::assignment_operator, "assignment_operator" },

		{ TokenKind::logical_operator, "logical_operator" },
		{ TokenKind::relational_operator, "relational_operator" },
		{ TokenKind::additive_operator, "additive_operator" },
		{ TokenKind::multiplicative_operator, "multiplicative_operator" },

		{ TokenKind::unary_operator, "unary_operator" },

		{ TokenKind::integer_literal, "integer_literal" },
		{ TokenKind::float_literal, "float_literal" },
		{ TokenKind::boolean_literal, "boolean_literal" },
		{ TokenKind::string_literal, "string_literal" },
		{ TokenKind::character_literal, "character_literal" }
	};

	/// <summary>
	/// Maps known symbols to their TokenKind enum values.
	/// Matches SYNTAX.txt.
	/// </summary>
	const std::unordered_map<std::string_view, TokenKind> KNOWN_SYMBOLS =
	{
		{ "exp", TokenKind::export_label },
		{ "pub", TokenKind::public_label },

		{ "extern", TokenKind::extern_keyword },
		{ "struct", TokenKind::struct_keyword },
		{ "enum", TokenKind::enum_keyword },
		{ "fn", TokenKind::function_keyword },

		{ "type", TokenKind::type_keyword },

		{ "var", TokenKind::variable_keyword },
		{ "const", TokenKind::constant_keyword },

		{ "resource", TokenKind::resource_keyword },
		{ "component", TokenKind::component_keyword },
		{ "query", TokenKind::query_keyword },
		{ "system", TokenKind::system_keyword },
		{ "group", TokenKind::group_keyword },
		{ "application", TokenKind::application_keyword },

		{ "require", TokenKind::require_keyword },
		{ "exclude", TokenKind::exclude_keyword },

		{ "for", TokenKind::for_keyword },
		{ "while", TokenKind::while_keyword },
		{ "if", TokenKind::if_keyword },
		{ "else", TokenKind::else_keyword },
		{ "return", TokenKind::return_keyword },

		{ "new", TokenKind::new_keyword },
		{ "move", TokenKind::move_keyword },

		{ "&", TokenKind::reference },
		//{ "*", TokenKind::pointer },

		{ ",", TokenKind::comma },
		{ ":", TokenKind::colon },
		{ ";", TokenKind::semicolon },
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

		{ "&&", TokenKind::logical_operator },
		{ "||", TokenKind::logical_operator },
		{ "==", TokenKind::relational_operator },
		{ "!=", TokenKind::relational_operator },
		{ "<", TokenKind::relational_operator },
		{ ">", TokenKind::relational_operator },
		{ "<=", TokenKind::relational_operator },
		{ ">=", TokenKind::relational_operator },
		{ "+", TokenKind::additive_operator },
		{ "-", TokenKind::additive_operator },
		{ "*", TokenKind::multiplicative_operator },
		{ "/", TokenKind::multiplicative_operator },
		{ "%", TokenKind::multiplicative_operator },

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
		{ '.', { } }		// . (... doesn't fit here as it is 3 characters long, case is handled manually in tokenizer)
	};

	/// <summary>
	/// Holds the data for all tokens in a source file.
	/// Token data can be accessed by the TokenID returned by AddToken, or through the iterator.
	/// </summary>
	class TokenBuffers
	{
	public:
		/// <summary>
		/// Utility class to iterate over the tokens in a TokenBuffers.
		/// </summary>
		class Iterator
		{
		public:
			Iterator(const TokenBuffers& tokenBuffers);

			/// <summary>
			/// Returns the TokenBuffers being iterated over.
			/// </summary>
			const TokenBuffers& GetTokenBuffers() const;

			/// <summary>
			/// Move to the next token.
			/// Returns false if there are no more tokens, true otherwise.
			/// </summary>
			[[nodiscard]] bool Next();

			/// <summary>
			/// Returns true if Next() will return true when next called.
			/// </summary>
			[[nodiscard]] bool HasNext() const;

			/// <summary>
			/// Move back to the previous token.
			/// </summary>
			void Previous();

			/// <summary>
			/// Returns the kind of the current token.
			/// </summary>
			TokenKind GetKind() const;

			/// <summary>
			/// Returns the string value of the current token.
			/// </summary>
			const std::string_view& GetValue() const;

			/// <summary>
			/// Returns the location in the file of the current token.
			/// </summary>
			const Location& GetLocation() const;

			/// <summary>
			/// Returns the string value of the line the current token is on.
			/// </summary>
			std::string_view GetLine() const;

			/// <summary>
			/// Returns the ID of the current token.
			/// </summary>
			TokenID GetCurrentID() const;

		private:
			const TokenBuffers& m_TokenBuffers;
			TokenID m_CurrentID;
		};

	public:
		TokenBuffers(const Source& source);

		Iterator GetIterator() const;

		/// <summary>
		/// Creates a new token and returns its ID.
		/// </summary>
		TokenID AddToken(TokenKind kind, const std::string_view& value, const Location& location);

		/// <summary>
		/// Returns the ID of the last token added.
		/// This is the token before EOF.
		/// </summary>
		TokenID LastTokenID() const;

		/// <summary>
		/// Returns the kind of the token with the given ID.
		/// </summary>
		TokenKind GetKind(TokenID id) const;

		/// <summary>
		/// Returns the string value of the token with the given ID.
		/// </summary>
		const std::string_view& GetValue(TokenID id) const;

		/// <summary>
		/// Returns the location in the file of the token with the given ID.
		/// </summary>
		const Location& GetLocation(TokenID id) const;

		/// <summary>
		/// Returns the string value of the line starting at the given character index.
		/// </summary>
		std::string_view GetLine(size_t startIndex) const;

	private:
		const Source& m_Source;

		std::vector<TokenKind> m_Kinds;
		std::vector<std::string_view> m_Values;
		std::vector<Location> m_Locations;
	};
}
