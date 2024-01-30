#include "Tokenizer.hpp"

#include <stack>

#include "log/Log.hpp"

namespace AlloyCompiler
{


	template <typename... Args>
	inline constexpr void logError(const SourceIterator& iter, const std::string& format, Args&&... args)
	{
		Log::Error("Error at location ({0} : {1}):", iter.CurrentLocation().Line, iter.CurrentLocation().Column);
		Log::Error("\t{0}", std::vformat(format, std::make_format_args(args...)));
	}

	inline static bool tryGetOperator(SourceIterator& iter, TokenBuffers& tokenBuffers)
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
		auto tokenString = iter.View(start, iter.CurrentIndex() + 1);

		tokenBuffers.AddToken(OPERATORS.at(tokenString), location, tokenString);

		return true;
	}

	inline static bool tryGetDelimiter(SourceIterator& iter, TokenBuffers& tokenBuffers)
	{
		// try to find the current character in the delimiters map
		auto it = DELIMITERS.find(iter.CurrentChar());

		if (it == DELIMITERS.end())
			return false;

		tokenBuffers.AddToken(it->second, iter.CurrentLocation(), iter.View(iter.CurrentIndex(), iter.CurrentIndex() + 1));
		return true;
	}

	inline static bool tryGetKeyword(SourceIterator& iter, TokenBuffers& tokenBuffers)
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
		std::string_view value = iter.View(start, iter.CurrentIndex());

		// check if word is any keyword
		auto it = KEYWORDS.find(value);
		if (it != KEYWORDS.end())
			tokenBuffers.AddToken(it->second, location, value);

		// otherwise, it's an identifier
		else
			tokenBuffers.AddToken(Token(TokenKind::Identifier, TokenValue::Identifier), location, value);

		// go back one character so the next loop can check it
		iter.PreviousChar();

		return true;
	}

	inline static bool tryGetString(SourceIterator& iter, TokenBuffers& tokenBuffers)
	{
		if (iter.CurrentChar() != '"')
			return false;

		// get the start index and location
		size_t start = iter.CurrentIndex();
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
		std::string_view value = iter.View(start, iter.CurrentIndex() + 1);

		tokenBuffers.AddToken(Token(TokenKind::Literal, TokenValue::String), location, value);
		return true;
	}

	inline static bool tryGetChar(SourceIterator& iter, TokenBuffers& tokenBuffers)
	{
		if (iter.CurrentChar() != '\'')
			return false;

		// get the start index and location
		size_t start = iter.CurrentChar();
		const Location& location = iter.CurrentLocation();

		// consume starting quote
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

		// consume ending quote
		if (!iter.NextChar())
		{
			logError(iter, "Unexpected end of file!");
			return false;
		}

		// store the character
		std::string_view value = iter.View(start, iter.CurrentIndex() + 1);
		tokenBuffers.AddToken(Token(TokenKind::Literal, TokenValue::Character), location, value);

		return true;
	}

	inline static bool tryGetNumber(SourceIterator& iter, TokenBuffers& tokenBuffers)
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
						iter.View(start, iter.CurrentIndex()));
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
		std::string_view value = iter.View(start, iter.CurrentIndex());

		// check if number is a float or int
		if (hasDot)
			tokenBuffers.AddToken(Token(TokenKind::Literal, TokenValue::Float), location, value);
		else
			tokenBuffers.AddToken(Token(TokenKind::Literal, TokenValue::Integer), location, value);

		// go back one character so the next loop can check it
		iter.PreviousChar();

		return true;
	}

	TokenBuffers Tokenize(const std::string_view& source)
	{
		SourceIterator iter(source);
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
			if (tryGetString(iter, tokenBuffers))
				continue;

			// check for character literals
			if (tryGetChar(iter, tokenBuffers))
				continue;

			// check for any numbers
			if (tryGetNumber(iter, tokenBuffers))
				continue;

			logError(iter, "Unexpected symbol '{0}'!", iter.CurrentChar());

		} while (iter.NextChar());

		// add end of file token
		tokenBuffers.AddToken(Token(TokenKind::EndOfFile, TokenValue::EndOfFile), iter.CurrentLocation(), "");

		return tokenBuffers;
	}
}


