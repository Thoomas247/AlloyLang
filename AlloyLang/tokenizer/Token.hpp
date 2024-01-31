#pragma once
#include <unordered_map>

#include "Source.hpp"

namespace AlloyCompiler
{
	/// <summary>
	/// Unique ID for a token. All token data is accessed through this ID.
	/// </summary>
	enum class TokenID : uint32_t {};

	/// <summary>
	/// Represents an invalid token ID.
	/// </summary>
	constexpr TokenID ERROR_TOKEN_ID = std::numeric_limits<TokenID>::max();

	/// <summary>
	/// TokenKind enum to match SYNTAX.txt.
	/// </summary>
	enum class TokenKind : uint8_t
	{
		none = 0,

		identifier,

		export_label,
		public_label,

		struct_keyword,
		enum_keyword,
		function_keyword,

		variable_keyword,
		constant_keyword,

		for_keyword,
		while_keyword,
		if_keyword,
		else_keyword,
		return_keyword,

		reference,
		pointer,

		comma,
		colon,
		semicolon,
		arrow,

		open_paren,
		close_paren,
		open_brace,
		close_brace,

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

		{ TokenKind::struct_keyword, "struct_keyword" },
		{ TokenKind::enum_keyword, "enum_keyword" },
		{ TokenKind::function_keyword, "function_keyword" },

		{ TokenKind::variable_keyword, "variable_keyword" },
		{ TokenKind::constant_keyword, "constant_keyword" },

		{ TokenKind::for_keyword, "for_keyword" },
		{ TokenKind::while_keyword, "while_keyword" },
		{ TokenKind::if_keyword, "if_keyword" },
		{ TokenKind::else_keyword, "else_keyword" },
		{ TokenKind::return_keyword, "return_keyword" },

		{ TokenKind::reference, "reference" },
		{ TokenKind::pointer, "pointer" },

		{ TokenKind::comma, "comma" },
		{ TokenKind::colon, "colon" },
		{ TokenKind::semicolon, "semicolon" },
		{ TokenKind::arrow, "arrow" },

		{ TokenKind::open_paren, "open_paren" },
		{ TokenKind::close_paren, "close_paren" },
		{ TokenKind::open_brace, "open_brace" },
		{ TokenKind::close_brace, "close_brace" },

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

		{ "struct", TokenKind::struct_keyword },
		{ "enum", TokenKind::enum_keyword },
		{ "fn", TokenKind::function_keyword },

		{ "var", TokenKind::variable_keyword },
		{ "const", TokenKind::constant_keyword },

		{ "for", TokenKind::for_keyword },
		{ "while", TokenKind::while_keyword },
		{ "if", TokenKind::if_keyword },
		{ "else", TokenKind::else_keyword },
		{ "return", TokenKind::return_keyword },

		{ "&", TokenKind::reference },
		{ "*", TokenKind::pointer },

		{ ",", TokenKind::comma },
		{ ":", TokenKind::colon },
		{ ";", TokenKind::semicolon },
		{ "->", TokenKind::arrow },

		{ "(", TokenKind::open_paren },
		{ ")", TokenKind::close_paren },
		{ "{", TokenKind::open_brace },
		{ "}", TokenKind::close_brace },

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
		{ "@", TokenKind::unary_operator },

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
		{ '-', { } },		// -
		{ '*', { } },		// *
		{ '/', { } },		// /
		{ '%', { } },		// %
		{ '@', { } }		// @
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
			/// Moves to the next token and checks if it matches one of the given kinds.
			/// If no match is found, an error is reported and the function returns false.
			/// If a match is found, returns true.
			/// </summary>
			[[nodiscard]] bool Expect(const std::vector<TokenKind>& options);

			/// <summary>
			/// Moves to the next token and checks if it matches one of the given values.
			/// If no match is found, an error is reported and the function returns false.
			/// If a match is found, returns true.
			/// </summary>
			[[nodiscard]] bool Expect(std::vector<std::string> options);

			/// <summary>
			/// Move to the next token.
			/// Returns false if there are no more tokens, true otherwise.
			/// </summary>
			[[nodiscard]] bool Next();

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
			const SmallStringView& GetValue() const;

			/// <summary>
			/// Returns the location in the file of the current token.
			/// </summary>
			const Location& GetLocation() const;

		private:
			/// <summary>
			/// Converts a vector of token kinds to a formatted list string.
			/// </summary>
			std::string tokenKindVectorToString(const std::vector<TokenKind>& tokens);

			/// <summary>
			/// Converts a vector of strings to a formatted list string.
			/// </summary>
			std::string stringVectorToString(const std::vector<std::string>& strings);

			/// <summary>
			/// Returns the string value of the line the current token is on.
			/// </summary>
			std::string_view getLine();

			/// <summary>
			/// Logs an error to the console which includes additional context, such as line number, column, etc...
			/// </summary>
			template <typename... Args>
			inline constexpr void logErrorAtPosition(const std::string& format, Args&&... args);

		private:
			const TokenBuffers& m_TokenBuffers;
			TokenID m_CurrentID;
		};

	public:
		Iterator GetIterator() const;

		/// <summary>
		/// Creates a new token and returns its ID.
		/// </summary>
		TokenID AddToken(TokenKind kind, const SmallStringView& value, const Location& location);

		/// <summary>
		/// Returns the ID of the last token added.
		/// </summary>
		TokenID LastTokenID() const;

		/// <summary>
		/// Returns the kind of the token with the given ID.
		/// </summary>
		TokenKind GetKind(TokenID id) const;

		/// <summary>
		/// Returns the string value of the token with the given ID.
		/// </summary>
		const SmallStringView& GetValue(TokenID id) const;

		/// <summary>
		/// Returns the location in the file of the token with the given ID.
		/// </summary>
		const Location& GetLocation(TokenID id) const;

	private:
		std::vector<TokenKind> m_Kinds;
		std::vector<SmallStringView> m_Values;
		std::vector<Location> m_Locations;
	};

	template<typename ...Args>
	inline constexpr void TokenBuffers::Iterator::logErrorAtPosition(const std::string& format, Args && ...args)
	{
		Log::Error("Error at location ({0} : {1}):", GetLocation().Line, GetLocation().Column);
		Log::Error("\t{0}", getLine());
		Log::Error("\t{0}^", std::string(GetLocation().Column - 1, ' '));
		Log::Error("\t{0}{1}", std::string(GetLocation().Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));
	}
}
