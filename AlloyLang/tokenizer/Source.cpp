#include "Source.hpp"

namespace AlloyCompiler
{
	Source::Source(const std::string_view& source)
		: m_SourceView(source)
	{}

	char Source::GetChar(size_t index) const
	{
		return m_SourceView[index];
	}

	size_t Source::GetSize() const
	{
		return m_SourceView.size();
	}

	std::string_view Source::GetLine(size_t startIndex) const
	{
		// walk forward to find the end of the line
		size_t end = startIndex;
		while (end < m_SourceView.size() && m_SourceView[end] != '\n')
		{
			++end;
		}

		return m_SourceView.substr(startIndex, end - startIndex);
	}

	std::string_view Source::CreateStringView(size_t start, size_t end) const
	{
		return m_SourceView.substr(start, end - start);
	}
}