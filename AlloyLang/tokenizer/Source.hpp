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
		class Iterator
		{
		public:
			Iterator(const Source& source);

			/// <summary>
			/// Moves to the next character and returns true if there is a next character.
			/// If there is no next character, returns false.
			/// </summary>
			bool NextChar();

			/// <summary>
			/// Returns true if there is a next character, otherwise returns false.
			/// </summary>
			bool HasNext() const;

			/// <summary>
			/// Returns the current character.
			/// </summary>
			char CurrentChar() const;

			/// <summary>
			/// Returns the character after the current character.
			/// </summary>
			char PeekNext() const;

			/// <summary>
			/// Returns the current index of the character in the source.
			/// </summary>
			size_t CurrentIndex() const;

			/// <summary>
			/// Returns the current location of the character in the source.
			/// </summary>
			Location CurrentLocation() const;

			/// <summary>
			/// Creates a std::string_view from the given start and end indices.
			/// </summary>
			std::string_view CreateStringView(size_t start, size_t end) const;

		private:
			const Source& m_Source;
			uint32_t m_CharIndex;
			uint32_t m_Line;
			uint32_t m_Column;

			std::stack<uint32_t> m_LineStarts;
		};

	public:
		Source(const std::string_view& source);

		/// <summary>
		/// Returns an iterator for the source.
		/// </summary>
		Iterator GetIterator() const;

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