#pragma once

#include <stack>

#include "../source/Source.hpp"
#include "Token.hpp"

namespace AlloyCompiler
{
	class TokenBuffer
	{
	public:
		TokenBuffer(const Source& source);

		void Tokenize();

		Token* GetToken(size_t index)
		{
			ASSERT(index < m_Tokens.size(), "Index is out of range!");

			return &m_Tokens.at(index);
		}

		size_t NumTokens() const
		{
			return m_Tokens.size();
		}

	private:
		template<typename... Args>
		constexpr void logErrorAtCurrentPosition(const std::string& format, Args&&... args);

		void addToken(TokenKind kind, const std::string_view& value, const Location& location);

		bool hasNext() const;
		bool eat();
		char current() const;
		char peek() const;
		size_t index() const;
		Location location() const;
		std::string_view createStringView(size_t start, size_t end) const;

		bool trySkipWhitespace();
		bool trySkipComment();
		bool tryGetOperator();
		bool tryGetDelimiter();
		bool tryGetKeyword();
		bool tryGetStringLiteral();
		bool tryGetCharLiteral();
		bool tryGetNumberLiteral();

	private:
		const Source& m_Source;
		std::vector<Token> m_Tokens;

		uint32_t m_CharIndex;
		uint32_t m_Line;
		uint32_t m_Column;

		std::stack<uint32_t> m_LineStarts;
	};

	template<typename ...Args>
	constexpr void TokenBuffer::logErrorAtCurrentPosition(const std::string& format, Args && ...args)
	{
		Log::Error("Error at location ({0} : {1}):", location().Line, location().Column);
		Log::Error("\t{0}", std::vformat(format, std::make_format_args(args...)));

		eat();
	}
}