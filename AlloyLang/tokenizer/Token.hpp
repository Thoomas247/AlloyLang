#pragma once
#include <unordered_map>

#include "util/SmallStringView.hpp"

namespace AlloyCompiler
{
	using TokenID = uint32_t;

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
			std::string tokenKindVectorToString(const std::vector<TokenKind>& tokens);
			std::string stringVectorToString(const std::vector<std::string>& strings);
			std::string_view getLine();

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
