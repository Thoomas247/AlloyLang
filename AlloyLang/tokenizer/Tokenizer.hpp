#pragma once
#include "Source.hpp"
#include "Token.hpp"

namespace AlloyCompiler
{

	class Tokenizer
	{
	public:
		Tokenizer(const Source& source);
		TokenBuffers Tokenize();

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
		TokenBuffers m_TokenBuffers;

		const Source& m_Source;
		uint32_t m_CharIndex;
		uint32_t m_Line;
		uint32_t m_Column;

		std::stack<uint32_t> m_LineStarts;
	};

	template<typename ...Args>
	constexpr void Tokenizer::logErrorAtCurrentPosition(const std::string& format, Args && ...args)
	{
		Log::Error("Error at location ({0} : {1}):", location().Line, location().Column);
		Log::Error("\t{0}", std::vformat(format, std::make_format_args(args...)));

		eat();
	}
}