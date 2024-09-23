#include "Source.hpp"

namespace AlloyCompiler
{
	Source::Source(const fs::path& filePath)
		: m_SourceFilePath(fs::absolute(filePath)), m_SourceString("")
	{}

	Source::Source(const std::string& sourceString)
		: m_SourceFilePath(""), m_SourceString(sourceString)
	{}

	bool Source::ReadFile()
	{
		if (!fs::exists(m_SourceFilePath))
		{
			Log::Error("Could not find file '{0}'.", m_SourceFilePath.string());
			return false;
		}

		std::ifstream fileStream(m_SourceFilePath);

		if (!fileStream.is_open())
		{
			Log::Error("Could not read file '{0}'.", m_SourceFilePath.string());
			return false;
		}

		std::stringstream stringStream;
		stringStream << fileStream.rdbuf();

		fileStream.close();

		m_SourceString = stringStream.str() + '\n';	// add an extra character at the end so that the tokenizer works properly

		return true;
	}

	char Source::GetChar(size_t index) const
	{
		ASSERT(!m_SourceString.empty(), "File has not been read! Call ReadFile() first.");

		return m_SourceString[index];
	}

	size_t Source::GetSize() const
	{
		ASSERT(!m_SourceString.empty(), "File has not been read! Call ReadFile() first.");

		return m_SourceString.size();
	}

	std::string_view Source::GetLine(size_t startIndex) const
	{
		ASSERT(!m_SourceString.empty(), "File has not been read! Call ReadFile() first.");

		// walk forward to find the end of the line
		size_t end = startIndex;
		while (end < m_SourceString.size() && m_SourceString[end] != '\n')
		{
			++end;
		}

		return std::string_view(m_SourceString).substr(startIndex, end - startIndex);
	}

	std::string_view Source::CreateStringView(size_t start, size_t end) const
	{
		ASSERT(!m_SourceString.empty(), "File has not been read! Call ReadFile() first.");

		return std::string_view(m_SourceString).substr(start, end - start);
	}
}