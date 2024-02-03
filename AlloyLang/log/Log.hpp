#pragma once
#include <iostream>
#include <regex>

namespace AlloyCompiler
{
	constexpr auto NUM_SPACES_PER_TAB = 2;

	namespace Log
	{
		inline static std::string convertTabsToSpaces(const std::string& s)
		{
			const std::regex tab(R"(\t)");
			const std::string spaces(NUM_SPACES_PER_TAB, ' ');

			return std::regex_replace(s, tab, spaces);
		}

		template <typename... Args>
		constexpr void Print(const std::string& format, Args&&... args)
		{
			std::cout << convertTabsToSpaces(std::vformat(format, std::make_format_args(args...))) << std::endl;
		}

		template <typename... Args>
		constexpr void Warn(const std::string& format, Args&&... args)
		{
			std::cerr << convertTabsToSpaces(std::vformat(format, std::make_format_args(args...))) << std::endl;
		}

		template <typename... Args>
		constexpr void Error(const std::string& format, Args&&... args)
		{
			std::cerr << convertTabsToSpaces(std::vformat(format, std::make_format_args(args...))) << std::endl;
		}

		template <typename... Args>
		constexpr void Fatal(const std::string& format, Args&&... args)
		{
			std::cerr << convertTabsToSpaces(std::vformat(format, std::make_format_args(args...))) << std::endl;
		}

	}

#ifdef _DEBUG

	template <typename... Args>
	constexpr void ASSERT(bool condition, const std::string& format, Args&&... args)
	{
		if (!condition)
		{
			Log::Fatal(format, args...);
			__debugbreak();
		}
	}

#else

	template <typename... Args>
	constexpr void ASSERT(bool condition, const std::string& format, Args&&... args) {}

#endif // DEBUG





}