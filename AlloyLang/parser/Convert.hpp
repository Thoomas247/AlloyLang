#pragma once

#include <string>

namespace AlloyCompiler
{
	int64_t StringToInt(const std::string_view& str)
	{
		return std::stol(std::string(str));
	}

	uint64_t StringToUInt(const std::string_view& str)
	{
		return std::stoul(std::string(str));
	}

	double StringToDouble(const std::string_view& str)
	{
		return std::stod(std::string(str));
	}
}