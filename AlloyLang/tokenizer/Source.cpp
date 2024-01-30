#include "Source.hpp"

namespace AlloyCompiler
{

	Source::Iterator::Iterator(const Source& source)
		: m_Source(source), m_CharIndex(0), m_Line(1), m_Column(1), m_LineStarts({ 0 })
	{}

	bool Source::Iterator::Expect()
	{
		if (!NextChar())
		{
			Log::Error("Unexpected end of file!");
			return false;
		}

		return true;
	}

	bool Source::Iterator::NextChar()
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
		if (m_CharIndex >= m_Source.GetSize())
			return false;

		return true;
	}

	char Source::Iterator::CurrentChar() const
	{
		return m_Source.GetChar(m_CharIndex);
	}

	void Source::Iterator::PreviousChar()
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

	size_t Source::Iterator::CurrentIndex() const { return m_CharIndex; }

	Location Source::Iterator::CurrentLocation() const
	{
		return Location(m_LineStarts.top(), m_Line, m_Column);
	}

	SmallStringView Source::Iterator::CreateSmallStringView(size_t start, size_t end) const
	{
		return m_Source.CreateSmallStringView(start, end);
	}

	Source::Source(const std::string_view& source)
		: m_SourceView(source)
	{}

	Source::Iterator Source::GetIterator() const
	{
		return Iterator(*this);
	}

	char Source::GetChar(size_t index) const
	{
		return m_SourceView[index];
	}

	size_t Source::GetSize() const
	{
		return m_SourceView.size();
	}

	SmallStringView Source::CreateSmallStringView(size_t start, size_t end) const
	{
		return m_SourceView.substr(start, end - start);
	}
}