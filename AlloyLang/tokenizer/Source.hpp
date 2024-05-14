#pragma once
#include <string_view>
#include <stack>

#include "../log/Log.hpp"

namespace AlloyCompiler
{
	/// <summary>
	/// Description of a token's location in the source code using line and column numbers.
	/// </summary>
	struct Location
	{
		uint32_t LineStart;
		uint32_t Line;
		uint32_t Column;

		constexpr explicit Location(uint32_t lineStart, uint32_t line, uint32_t column)
			: LineStart(lineStart), Line(line), Column(column)
		{}
	};

	/// <summary>
	/// Holds the source code and provides an iterator for it.
	/// </summary>
	class Source
	{
	public:
		Source(const std::string_view& source);

		/// <summary>
		/// Returns the character at the given index.
		/// </summary>
		char GetChar(size_t index) const;

		/// <summary>
		/// Returns the size of the source.
		/// </summary>
		size_t GetSize() const;

		/// <summary>
		/// Returns the line that the character at the given index is on.
		/// </summary>
		std::string_view GetLine(size_t startIndex) const;

		/// <summary>
		/// Creates a std::string_view from the given start and end indices.
		/// </summary>
		std::string_view CreateStringView(size_t start, size_t end) const;

	private:
		std::string_view m_SourceView;
	};

}