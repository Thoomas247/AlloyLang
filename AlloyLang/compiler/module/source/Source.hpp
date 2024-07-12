#pragma once
#include <string_view>
#include <filesystem>
#include <fstream>

#include "../../../log/Log.hpp"

namespace AlloyCompiler
{
	namespace fs = std::filesystem;

	/// <summary>
	/// Description of a character's location in the source code using line and column numbers.
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

	class Source
	{
	public:
		Source(const fs::path& filePath);

		/// <summary>
		/// Opens and reads the file at the given filepath.
		/// Returns true on success, false otherwise.
		/// </summary>
		bool ReadFile();

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

		/// <summary>
		/// Set source to a string rather than a file
		/// </summary>
		void SetSourceString(const std::string& source) { m_SourceString = source; }

	private:
		fs::path m_SourceFilePath;
		std::string m_SourceString;
	};

}