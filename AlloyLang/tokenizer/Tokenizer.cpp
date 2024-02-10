#include "Tokenizer.hpp"

#include <stack>

#include "../log/Log.hpp"

namespace AlloyCompiler
{

	template <typename... Args>
	inline constexpr void logError(const Source::Iterator& iter, const std::string& format, Args&&... args)
	{
		Log::Error("Error at location ({0} : {1}):", iter.CurrentLocation().Line, iter.CurrentLocation().Column);
		Log::Error("\t{0}", std::vformat(format, std::make_format_args(args...)));
	}

	bool tryGetOperator(Source::Iterator& iter, TokenBuffers& tokenBuffers)
	{
		// look for the current character in the operators map
		auto it = OPERATOR_COMBINATIONS.find(iter.CurrentChar());
		if (it == OPERATOR_COMBINATIONS.end())
			return false;

		// store the starting index and position
		size_t start = iter.CurrentIndex();
		const Location& location = iter.CurrentLocation();

		// get the next character
		if (!iter.NextChar())
		{
			logError(iter, "Unexpected end of file!");
			return false;
		}

		// check the next character to see if it's also part of the operator
		bool found = false;
		for (char c : it->second)
		{
			if (iter.CurrentChar() == c)
				found = true;
		}

		// if the next character is not part of the operator, go back one character
		if (!found)
			iter.PreviousChar();

		// get the operator string
		auto tokenString = iter.CreateSmallStringView(start, iter.CurrentIndex() + 1);

		tokenBuffers.AddToken(KNOWN_SYMBOLS.at(tokenString.ToStringView()), tokenString, location);

		return true;
	}

	bool tryGetDelimiter(Source::Iterator& iter, TokenBuffers& tokenBuffers)
	{
		// try to find the current character in the map
		auto it = KNOWN_SYMBOLS.find(std::string(1, iter.CurrentChar()));

		if (it == KNOWN_SYMBOLS.end())
			return false;

		tokenBuffers.AddToken(it->second, iter.CreateSmallStringView(iter.CurrentIndex(), iter.CurrentIndex() + 1), iter.CurrentLocation());
		return true;
	}

	bool tryGetKeyword(Source::Iterator& iter, TokenBuffers& tokenBuffers)
	{
		if (!isalpha(iter.CurrentChar()) && iter.CurrentChar() != '_')
			return false;

		// get the start index and location
		size_t start = iter.CurrentIndex();
		const Location& location = iter.CurrentLocation();

		// keep going until the end of the word
		do
		{
			if (!iter.NextChar())
			{
				logError(iter, "Unexpected end of file!");
				return false;
			}
		} while (isalnum(iter.CurrentChar()) || iter.CurrentChar() == '_');	// after the first character, can also be a number

		// store the word
		auto value = iter.CreateSmallStringView(start, iter.CurrentIndex());

		// check if word is any keyword
		auto it = KNOWN_SYMBOLS.find(value.ToStringView());
		if (it != KNOWN_SYMBOLS.end())
			tokenBuffers.AddToken(it->second, value, location);

		// otherwise, it's an identifier
		else
			tokenBuffers.AddToken(TokenKind::identifier, value, location);

		// go back one character so the next loop can check it
		iter.PreviousChar();

		return true;
	}

	bool tryGetStringLiteral(Source::Iterator& iter, TokenBuffers& tokenBuffers)
	{
		if (iter.CurrentChar() != '"')
			return false;

		// get the start index and location
		size_t start = iter.CurrentIndex() + 1;
		const Location& location = iter.CurrentLocation();

		// keep going until the end of the string
		do
		{
			if (!iter.NextChar())
			{
				logError(iter, "Unexpected end of file!");
				return false;
			}

			// check for escape character
			if (iter.CurrentChar() == '\\')
			{
				if (!iter.NextChar())
				{
					logError(iter, "Unexpected end of file!");
					return false;
				}
			}

		} while (iter.CurrentChar() != '"');

		// store the string
		auto value = iter.CreateSmallStringView(start, iter.CurrentIndex());

		tokenBuffers.AddToken(TokenKind::string_literal, value, location);
		return true;
	}

	bool tryGetCharLiteral(Source::Iterator& iter, TokenBuffers& tokenBuffers)
	{
		if (iter.CurrentChar() != '\'')
			return false;

		// get the  location
		const Location& location = iter.CurrentLocation();

		// consume starting quote
		if (!iter.NextChar())
		{
			logError(iter, "Unexpected end of file!");
			return false;
		}

		// get the start index
		const size_t start = iter.CurrentIndex();

		// check for escape character
		if (iter.CurrentChar() == '\\')
		{
			if (!iter.NextChar())
			{
				logError(iter, "Unexpected end of file!");
				return false;
			}
		}

		// consume ending quote
		if (!iter.NextChar())
		{
			logError(iter, "Unexpected end of file!");
			return false;
		}

		// store the character
		auto value = iter.CreateSmallStringView(start, iter.CurrentIndex());
		tokenBuffers.AddToken(TokenKind::character_literal, value, location);

		return true;
	}

	bool tryGetNumberLiteral(Source::Iterator& iter, TokenBuffers& tokenBuffers)
	{
		if (!isdigit(iter.CurrentChar()))
			return false;

		// get the start index and location
		size_t start = iter.CurrentIndex();
		const Location& location = iter.CurrentLocation();

		bool hasDot = false;

		// keep going until the end of the number
		do
		{
			if (!iter.NextChar())
			{
				logError(iter, "Unexpected end of file!");
				return false;
			}

			if (iter.CurrentChar() == '.')
			{
				if (hasDot)
				{
					logError(iter,
						"Invalid float literal: {0}\nExpected only one decimal point in float literal.",
						iter.CreateSmallStringView(start, iter.CurrentIndex()).ToStringView());
					return false;
				}

				hasDot = true;
				if (!iter.NextChar())
				{
					logError(iter, "Unexpected end of file!");
					return false;
				}
			}

		} while (isdigit(iter.CurrentChar()));

		// store the number
		auto value = iter.CreateSmallStringView(start, iter.CurrentIndex());

		// check if number is a float or int
		if (hasDot)
			tokenBuffers.AddToken(TokenKind::float_literal, value, location);
		else
			tokenBuffers.AddToken(TokenKind::integer_literal, value, location);

		// go back one character so the next loop can check it
		iter.PreviousChar();

		return true;
	}

	TokenBuffers Tokenize(const Source& source)
	{
		Source::Iterator iter = source.GetIterator();
		TokenBuffers tokenBuffers(source);

		do
		{
			// skip whitespace
			if (isspace(iter.CurrentChar()))
				continue;

			// check for any operators
			if (tryGetOperator(iter, tokenBuffers))
				continue;

			// check for any delimiters
			if (tryGetDelimiter(iter, tokenBuffers))
				continue;

			// check for keywords
			if (tryGetKeyword(iter, tokenBuffers))
				continue;

			// check for string literals
			if (tryGetStringLiteral(iter, tokenBuffers))
				continue;

			// check for character literals
			if (tryGetCharLiteral(iter, tokenBuffers))
				continue;

			// check for any numbers
			if (tryGetNumberLiteral(iter, tokenBuffers))
				continue;

			logError(iter, "Unexpected symbol '{0}'!", iter.CurrentChar());

		} while (iter.NextChar());

		return tokenBuffers;
	}

	void PrintTokens(const TokenBuffers& tokenBuffers)
	{
		TokenBuffers::Iterator iter(tokenBuffers);

		while (iter.Next())
		{
			Log::Print("{0} ({1})", TOKEN_KIND_NAMES.at(iter.GetKind()), iter.GetValue().ToStringView());
		}
	}
}


