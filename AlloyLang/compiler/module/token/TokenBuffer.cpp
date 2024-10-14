#include "TokenBuffer.hpp"

#ifdef CLR
#include <msclr\marshal_cppstd.h>
#endif


namespace AlloyCompiler
{
#ifdef CLR
	//
	// split a string into multiple tokens and return the token list to the calling .Net object
	//
	/* static*/
	System::Collections::Generic::List<GlobalFunctions::CSharpToken^>^ GlobalFunctions::Tokenize(System::String^ managedStr)
	{
		System::Collections::Generic::List<GlobalFunctions::CSharpToken^>^ list;
		if (!System::String::IsNullOrEmpty(managedStr))
		{
			msclr::interop::marshal_context context;
			AlloyCompiler::Source src(context.marshal_as<std::string>(managedStr));
			AlloyCompiler::TokenBuffer tok(src);
			tok.Tokenize();
			int count = (int)tok.NumTokens();
			list = gcnew System::Collections::Generic::List<GlobalFunctions::CSharpToken^>(count);
			for (int n = 0; n < count; n++)
			{
				// convert our C++ tokens into C# token and add it to the list
				Token* token = tok.GetToken(n);
				GlobalFunctions::CSharpToken^ cstoken = gcnew GlobalFunctions::CSharpToken();
				cstoken->Value = gcnew System::String(std::string(token->Value).data());
				cstoken->LineStart = token->Location.LineStart;
				cstoken->Line = token->Location.Line;
				cstoken->Column = token->Location.Column;
				cstoken->Kind = (int)token->Kind;
				list->Add(cstoken);
			}
		}
		return list;
	}
#endif

	TokenBuffer::TokenBuffer(const Source& source)
		: m_Source(source)
		, m_CharIndex(0)
		, m_Column(1)
		, m_Line(1)
		, m_LineStarts({ 0 })
	{
	}

	bool TokenBuffer::Tokenize()
	{
		do
		{
			// skip whitespace
			if (trySkipWhitespace())
				continue;

			// skip comments
			if (tryGetComment())
				continue;

			// check for any operators
			if (tryGetOperator())
				continue;

			// check for any delimiters
			if (tryGetDelimiter())
				continue;

			// check for keywords
			if (tryGetKeyword())
				continue;

			// check for string literals
			if (tryGetStringLiteral())
				continue;

			// check for character literals
			if (tryGetCharLiteral())
				continue;

			// check for any numbers
			if (tryGetNumberLiteral())
				continue;

			logErrorAtCurrentPosition("Unexpected symbol '{0}'!", current());
			return false;

		} while (hasNext());

		addToken(TokenKind::end_of_file, createStringView(index(), index()), location());
		return true;
	}

	Token* TokenBuffer::GetToken(size_t index)
	{
		ASSERT(index < m_Tokens.size(), "Index is out of range!");

		return &m_Tokens.at(index);
	}

	size_t TokenBuffer::NumTokens() const
	{
		return m_Tokens.size();
	}

	void TokenBuffer::addToken(TokenKind kind, const std::string_view& value, const Location& location)
	{
		m_Tokens.push_back(Token{ .Value = value, .Location = location, .Kind = kind });
	}

	bool TokenBuffer::hasNext() const
	{
		return m_CharIndex < m_Source.GetSize() - 1;
	}

	bool TokenBuffer::eat()
	{
		// check if we have a new line
		if (current() == '\n')
		{
			m_LineStarts.push(m_CharIndex + 1);
			++m_Line;
			m_Column = 1;
		}
		else
		{
			++m_Column;
		}
		
		++m_CharIndex;

		// check that we haven't reached the end of the file
		if (m_CharIndex >= m_Source.GetSize())
			return false;

		return true;
	}

	char TokenBuffer::current() const
	{
		return m_Source.GetChar(m_CharIndex);
	}

	char TokenBuffer::peek() const
	{
		ASSERT(hasNext(), "TokenBuffer::peek() called when there is no next character!");

		return m_Source.GetChar((size_t)m_CharIndex + 1);
	}

	size_t TokenBuffer::index() const
	{
		return (size_t)m_CharIndex;
	}

	Location TokenBuffer::location() const
	{
		return Location(m_LineStarts.top(), m_Line, m_Column);
	}

	std::string_view TokenBuffer::createStringView(size_t start, size_t end) const
	{
		return m_Source.CreateStringView(start, end);
	}

	bool TokenBuffer::trySkipWhitespace()
	{
		// check if we have a whitespace
		if (!isspace(current()))
			return false;

		do
		{
			if (!eat())
			{
				break;
			}
		} while (isspace(current()));

		return true;
	}

	bool TokenBuffer::tryGetComment()
	{
		if (current() != '/' || !hasNext())
		{
			return false;
		}

		// store the starting index and position
		size_t start = index();
		const Location& loc = location();

		// check if we have a single line comment
		if (peek() == '/')
		{
			// consume first two characters
			eat();
			eat();

			// consume characters until the end of the line
			while (hasNext() && current() != '\n')
			{
				eat();
			}
		}

		// check if we have a multi line comment
		else if (peek() == '*')
		{
			// consume the first two characters
			eat();
			eat();

			size_t depth = 1;
			while (hasNext() && depth > 0)
			{
				if (current() == '/' && peek() == '*')
				{
					++depth;
					eat();
				}
				else if (current() == '*' && peek() == '/')
				{
					--depth;
					eat();
				}

				eat();
			}

			if (depth > 0)
			{
				logErrorAtCurrentPosition("Unexpected end of file! Missing '*/'.");
			}
		}

		// otherwise, this isn't a comment
		else
		{
			return false;
		}

		// get the comment string
		auto tokenString = createStringView(start, index());

		addToken(TokenKind::comment, tokenString, loc);

		return true;
	}

	bool TokenBuffer::tryGetOperator()
	{
		// look for the current character in the operators map
		auto it = OPERATOR_COMBINATIONS.find(current());
		if (it == OPERATOR_COMBINATIONS.end())
			return false;

		// store the starting index and position
		size_t start = index();
		const Location& loc = location();

		// special case for '...' since it is 3 characters long
		if (current() == '.')
		{
			eat();	// consume .

			if (current() == '.' && peek() == '.')
			{
				eat();
				eat();
			}
		}

		// check all other 2 character operators
		else
		{
			eat();	// consume any first character

			for (char c : it->second)
			{
				if (current() == c)
				{
					eat();
					break;
				}
			}
		}

		// get the operator string
		auto tokenString = createStringView(start, index());

		// check if the operator is in the known symbols map
		ASSERT(KNOWN_SYMBOLS.contains(tokenString), "Unknown operator: {0}", tokenString);

		// get the token kind
		TokenKind kind = KNOWN_SYMBOLS.at(tokenString);

		addToken(kind, tokenString, loc);

		return true;
	}

	bool TokenBuffer::tryGetDelimiter()
	{
		// try to find the current character in the map
		auto it = KNOWN_SYMBOLS.find(std::string(1, current()));

		if (it == KNOWN_SYMBOLS.end())
			return false;

		addToken(it->second, createStringView(index(), index() + 1), location());

		// consume the delimiter
		eat();

		return true;
	}

	bool TokenBuffer::tryGetKeyword()
	{
		if (!isalpha(current()) && current() != '_')
			return false;

		// get the start index and location
		size_t start = index();
		const Location& loc = location();

		// consume the first character
		eat();

		bool isLongIdentifier = false;
		bool reading = true;
		bool foundLetters = false;

		while (reading)
		{
			// keep going until the end of the word
			while (isalnum(current()) || current() == '_')
			{
				foundLetters = true;
				eat();
			}

			if (current() == ':' && peek() == ':')
			{
				isLongIdentifier = true;
				foundLetters = false;
				eat();
				eat();
			}
			else
			{
				reading = false;
			}
		}

		// ensure we don't end on a ::
		if (isLongIdentifier && !foundLetters)
		{
			logErrorAtCurrentPosition("Expected an identifier.");
		}

		// store the word
		auto value = createStringView(start, index());

		// check if word is any keyword
		auto it = KNOWN_SYMBOLS.find(value);
		if (it != KNOWN_SYMBOLS.end())
		{
			addToken(it->second, value, loc);
		}

		// otherwise, it's an identifier
		else
		{
			if (isLongIdentifier)
			{
				addToken(TokenKind::long_identifier, value, loc);
			}
			else
			{
				addToken(TokenKind::identifier, value, loc);
			}
		}

		return true;
	}

	bool TokenBuffer::tryGetStringLiteral()
	{
		if (current() != '"')
			return false;

		// get the start index and location
		size_t start = index() + 1;
		const Location& loc = location();

		// keep going until the end of the string
		do
		{
			if (!eat())
			{
				logErrorAtCurrentPosition("Unexpected end of file!");
				return false;
			}

			// check for escape character
			if (current() == '\\')
			{
				if (!eat())
				{
					logErrorAtCurrentPosition("Unexpected end of file!");
					return false;
				}
			}

		} while (current() != '"');

		// store the string
		auto value = createStringView(start, index());

		// consume the ending quote
		eat();

		addToken(TokenKind::string_literal, value, loc);
		return true;
	}

	bool TokenBuffer::tryGetCharLiteral()
	{
		if (current() != '\'')
			return false;

		// get the location
		const Location& loc = location();

		// consume starting quote
		if (!eat())
		{
			logErrorAtCurrentPosition("Unexpected end of file!");
			return false;
		}

		// get the start index
		const size_t start = index();

		// check for escape character
		if (current() == '\\')
		{
			if (!eat())
			{
				logErrorAtCurrentPosition("Unexpected end of file!");
				return false;
			}
		}

		// consume the character
		if (!eat())
		{
			logErrorAtCurrentPosition("Unexpected end of file!");
			return false;
		}

		// store the character
		auto value = createStringView(start, index());

		// consume ending quote
		if (!eat())
		{
			logErrorAtCurrentPosition("Unexpected end of file!");
			return false;
		}

		addToken(TokenKind::character_literal, value, loc);

		return true;
	}

	bool TokenBuffer::tryGetNumberLiteral()
	{
		if (!isdigit(current()))
			return false;

		// get the start index and location
		size_t start = index();
		const Location& loc = location();

		bool hasDot = false;

		// keep going until the end of the number
		while (isdigit(current()) || current() == '.')
		{
			if (current() == '.')
			{
				if (hasDot)
				{
					logErrorAtCurrentPosition(
						"Invalid float literal: {0}. Expected only one decimal point in float literal.",
						createStringView(start, index()));
					return false;
				}

				hasDot = true;
			}

			// consume the digit or dot
			eat();
		}

		// store the number
		auto value = createStringView(start, index());

		// check if number is a float or int
		if (hasDot)
			addToken(TokenKind::float_literal, value, loc);
		else
			addToken(TokenKind::integer_literal, value, loc);

		return true;
	}

}
