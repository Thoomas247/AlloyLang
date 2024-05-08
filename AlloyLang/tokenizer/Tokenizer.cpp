#include "Tokenizer.hpp"

#include <stack>

#include "../log/Log.hpp"

namespace AlloyCompiler
{

	template <typename... Args>
	inline constexpr void logError(Source::Iterator& iter, const std::string& format, Args&&... args)
	{
		Log::Error("Error at location ({0} : {1}):", iter.CurrentLocation().Line, iter.CurrentLocation().Column);
		Log::Error("\t{0}", std::vformat(format, std::make_format_args(args...)));

		iter.NextChar();
	}

	bool trySkipWhitespace(Source::Iterator& iter)
	{
		// check if we have a whitespace
		if (!isspace(iter.CurrentChar()))
			return false;

		do
		{
			if (!iter.NextChar())
			{
				break;
			}
		} while (isspace(iter.CurrentChar()));

		return true;
	}

	bool trySkipComment(Source::Iterator& iter)
	{
		if (iter.CurrentChar() != '/' || !iter.HasNext())
		{
			return false;
		}

		// check if we have a single line comment
		if (iter.PeekNext() == '/')
		{
			// consume first two characters
			iter.NextChar();
			iter.NextChar();

			// consume characters until the end of the line
			while (iter.HasNext() && iter.CurrentChar() != '\n')
			{
				iter.NextChar();
			}

			return true;
		}

		// check if we have a multi line comment
		else if (iter.PeekNext() == '*')
		{
			// consume the first two characters
			iter.NextChar();
			iter.NextChar();

			size_t depth = 1;
			while (iter.HasNext() && depth > 0)
			{
				if (iter.CurrentChar() == '/' && iter.PeekNext() == '*')
				{
					++depth;
					iter.NextChar();
				}
				else if (iter.CurrentChar() == '*' && iter.PeekNext() == '/')
				{
					--depth;
					iter.NextChar();
				}

				iter.NextChar();
			}

			if (depth > 0)
			{
				logError(iter, "Unexpected end of file! Missing '*/'.");
			}

			return true;
		}
		else
		{
			return false;
		}
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

		// special case for '...' since it is 3 characters long
		if (iter.CurrentChar() == '.')
		{
			iter.NextChar();	// consume .

			if (iter.CurrentChar() == '.' && iter.PeekNext() == '.')
			{
				iter.NextChar();
				iter.NextChar();
			}
		}

		// check all other 2 character operators
		else
		{
			iter.NextChar();	// consume any first character

			for (char c : it->second)
			{
				if (iter.CurrentChar() == c)
				{
					iter.NextChar();
					break;
				}
			}
		}

		// get the operator string
		auto tokenString = iter.CreateStringView(start, iter.CurrentIndex());

		// check if the operator is in the known symbols map
		ASSERT(KNOWN_SYMBOLS.contains(tokenString), "Unknown operator: {0}", tokenString);

		// get the token kind
		TokenKind kind = KNOWN_SYMBOLS.at(tokenString);

		tokenBuffers.AddToken(kind, tokenString, location);

		return true;
	}

	bool tryGetDelimiter(Source::Iterator& iter, TokenBuffers& tokenBuffers)
	{
		// try to find the current character in the map
		auto it = KNOWN_SYMBOLS.find(std::string(1, iter.CurrentChar()));

		if (it == KNOWN_SYMBOLS.end())
			return false;

		tokenBuffers.AddToken(it->second, iter.CreateStringView(iter.CurrentIndex(), iter.CurrentIndex() + 1), iter.CurrentLocation());

		// consume the delimiter
		iter.NextChar();

		return true;
	}

	bool tryGetKeyword(Source::Iterator& iter, TokenBuffers& tokenBuffers)
	{
		if (!isalpha(iter.CurrentChar()) && iter.CurrentChar() != '_')
			return false;

		// get the start index and location
		size_t start = iter.CurrentIndex();
		const Location& location = iter.CurrentLocation();

		// consume the first character
		iter.NextChar();

		// keep going until the end of the word
		while (isalnum(iter.CurrentChar()) || iter.CurrentChar() == '_')
		{
			iter.NextChar();
		}

		// store the word
		auto value = iter.CreateStringView(start, iter.CurrentIndex());

		// check if word is any keyword
		auto it = KNOWN_SYMBOLS.find(value);
		if (it != KNOWN_SYMBOLS.end())
			tokenBuffers.AddToken(it->second, value, location);

		// otherwise, it's an identifier
		else
			tokenBuffers.AddToken(TokenKind::identifier, value, location);

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
		auto value = iter.CreateStringView(start, iter.CurrentIndex());

		// consume the ending quote
		iter.NextChar();

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

		// consume the character
		if (!iter.NextChar())
		{
			logError(iter, "Unexpected end of file!");
			return false;
		}

		// store the character
		auto value = iter.CreateStringView(start, iter.CurrentIndex());

		// consume ending quote
		if (!iter.NextChar())
		{
			logError(iter, "Unexpected end of file!");
			return false;
		}

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
		while (isdigit(iter.CurrentChar()) || iter.CurrentChar() == '.')
		{
			if (iter.CurrentChar() == '.')
			{
				if (hasDot)
				{
					logError(iter,
						"Invalid float literal: {0}. Expected only one decimal point in float literal.",
						iter.CreateStringView(start, iter.CurrentIndex()));
					return false;
				}

				hasDot = true;
			}

			// consume the digit or dot
			iter.NextChar();
		}

		// store the number
		auto value = iter.CreateStringView(start, iter.CurrentIndex());

		// check if number is a float or int
		if (hasDot)
			tokenBuffers.AddToken(TokenKind::float_literal, value, location);
		else
			tokenBuffers.AddToken(TokenKind::integer_literal, value, location);

		return true;
	}

	TokenBuffers Tokenize(const Source& source)
	{
		Source::Iterator iter = source.GetIterator();
		TokenBuffers tokenBuffers(source);

		do
		{
			// skip whitespace
			if (trySkipWhitespace(iter))
				continue;

			// skip comments
			if (trySkipComment(iter))
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

		} while (iter.HasNext());

		tokenBuffers.AddToken(TokenKind::end_of_file, iter.CreateStringView(iter.CurrentIndex(), iter.CurrentIndex()), iter.CurrentLocation());

		return tokenBuffers;
	}

	void PrintTokens(const TokenBuffers& tokenBuffers)
	{
		TokenBuffers::Iterator iter(tokenBuffers);

		do
		{
			Log::Print("{0} ({1})", TOKEN_KIND_NAMES.at(iter.GetKind()), iter.GetValue());
		} while (iter.Next());
	}
}


