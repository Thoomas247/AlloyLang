#pragma once
#include <string_view>

#include "../log/Log.hpp"

namespace AlloyCompiler
{
	/// <summary>
	/// Alternative to std::string_view for small strings.
	/// Used to keep token data compact.
	/// </summary>
	class SmallStringView
	{
	public:
		using size_type = uint8_t;

		static constexpr auto MAX_SIZE = std::numeric_limits<size_type>::max();

		constexpr SmallStringView(const std::string_view& sv)
			: m_pData(sv.data()), m_Size((size_type)sv.size())
		{
			ASSERT(sv.size() <= MAX_SIZE,
				"Given string_view is too large to store in SmallStringView!");
		}

		const char* Data() const
		{
			return m_pData;
		}

		size_type Size() const
		{
			return m_Size;
		}

		std::string_view ToStringView() const
		{
			return std::string_view(m_pData, m_Size);
		}

		char operator[](size_type index) const
		{
			ASSERT(index < m_Size, "Index out of range!");
			return m_pData[index];
		}

		bool operator==(const SmallStringView& other) const
		{
			return ToStringView() == other.ToStringView();
		}

		bool operator==(const std::string_view& other) const
		{
			return ToStringView() == other;
		}

	private:
		const char* m_pData;
		const size_type m_Size;
	};

}
