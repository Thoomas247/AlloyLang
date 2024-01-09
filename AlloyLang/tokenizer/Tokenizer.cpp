#include "Tokenizer.hpp"

#include <stack>

#include "log/Log.hpp"
#include "TokenMaps.hpp"

namespace AlloyCompiler::Tokenizer
{
	class SourceIterator
	{
	public:
		SourceIterator(const std::string_view& source)
			: m_SourceView(source), m_CharIndex(0), m_Line(1), m_Column(1), m_LineStarts({ 0 })
		{}

		bool NextChar()
		{
			// check if we have a new line
			if (CurrentChar() == '\n')
			{
				m_LineStarts.push(m_CharIndex + 1);
				++m_Line;
				m_Column = 1;
			}

			// check if we have a tab
			else if (CurrentChar() == '\t')
			{
				m_Column += NUM_SPACES_PER_TAB;
			}
			else
			{
				++m_Column;
			}

			++m_CharIndex;

			// check that we haven't reached the end of the file
			if (m_CharIndex >= m_SourceView.size())
				return false;

			return true;
		}

		char CurrentChar() const
		{
			return m_SourceView[m_CharIndex];
		}

		void PreviousChar()
		{
			ASSERT(m_CharIndex > 0, "Tokenizer::lastChar() called when current char position is 0!");

			--m_CharIndex;

			// if current character is a new line, go back one line
			if (CurrentChar() == '\n')
			{
				m_LineStarts.pop();
				--m_Line;
				m_Column = 1;
			}

			// if current character is a tab, go back four columns
			else if (CurrentChar() == '\t')
			{
				m_Column -= NUM_SPACES_PER_TAB;
			}

			else
			{
				--m_Column;
			}
		}

		size_t CurrentIndex() const { return m_CharIndex; }

		Location CurrentLocation() const
		{
			return Location(m_LineStarts.top(), m_Line, m_Column);
		}

		SourceView View(size_t start, size_t end) const
		{
			return m_SourceView.substr(start, end - start);
		}

	private:
		SourceView m_SourceView;
		uint32_t m_CharIndex;
		uint32_t m_Line;
		uint32_t m_Column;

		std::stack<uint32_t> m_LineStarts;
	};

	template <typename... Args>
	inline constexpr void logError(const SourceIterator& iter, const std::string& format, Args&&... args)
	{
		Log::Error("Error at location ({0} : {1}):", iter.CurrentLocation().Line, iter.CurrentLocation().Column);
		Log::Error("\t{0}", std::vformat(format, std::make_format_args(args...)));
	}

	inline static bool tryGetOperator(SourceIterator& iter, TokenDataBuffers& buffers)
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

		buffers.AddToken(OPERATORS.at(tokenString), location, tokenString);

		return true;
	}

	inline static bool tryGetDelimiter(SourceIterator& iter, TokenDataBuffers& buffers)
	{
		// try to find the current character in the delimiters map
		auto it = DELIMITERS.find(iter.CurrentChar());

		if (it == DELIMITERS.end())
			return false;

		buffers.AddToken(it->second, iter.CurrentLocation(), iter.View(iter.CurrentIndex(), iter.CurrentIndex() + 1));
		return true;
	}

	inline static bool tryGetKeyword(SourceIterator& iter, TokenDataBuffers& buffers)
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
			buffers.AddToken(it->second, location, value);

		// otherwise, it's an identifier
		else
			buffers.AddToken(Token(TokenKind::Identifier, TokenValue::Identifier), location, value);

		// go back one character so the next loop can check it
		iter.PreviousChar();

		return true;
	}

	inline static bool tryGetString(SourceIterator& iter, TokenDataBuffers& buffers)
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

		buffers.AddToken(Token(TokenKind::Literal, TokenValue::String), location, value);
		return true;
	}

	inline static bool tryGetChar(SourceIterator& iter, TokenDataBuffers& buffers)
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
		buffers.AddToken(Token(TokenKind::Literal, TokenValue::Character), location, value);

		return true;
	}

	inline static bool tryGetNumber(SourceIterator& iter, TokenDataBuffers& buffers)
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
			buffers.AddToken(Token(TokenKind::Literal, TokenValue::Float), location, value);
		else
			buffers.AddToken(Token(TokenKind::Literal, TokenValue::Integer), location, value);

		// go back one character so the next loop can check it
		iter.PreviousChar();

		return true;
	}

	TokenDataBuffers Tokenize(const std::string_view& source)
	{
		SourceIterator iter(source);
		TokenDataBuffers buffers(source);

		do
		{
			// skip whitespace
			if (isspace(iter.CurrentChar()))
				continue;

			// check for any operators
			if (tryGetOperator(iter, buffers))
				continue;

			// check for any delimiters
			if (tryGetDelimiter(iter, buffers))
				continue;

			// check for keywords
			if (tryGetKeyword(iter, buffers))
				continue;

			// check for string literals
			if (tryGetString(iter, buffers))
				continue;

			// check for character literals
			if (tryGetChar(iter, buffers))
				continue;

			// check for any numbers
			if (tryGetNumber(iter, buffers))
				continue;

			logError(iter, "Unexpected symbol '{0}'!", iter.CurrentChar());

		} while (iter.NextChar());

		// add end of file token
		buffers.AddToken(Token(TokenKind::EndOfFile, TokenValue::EndOfFile), iter.CurrentLocation(), "");

		return buffers;
	}
}


