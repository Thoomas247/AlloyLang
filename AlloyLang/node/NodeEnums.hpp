#pragma once
#include <cstdint>

namespace AlloyCompiler
{
	enum class TypeModifier : uint8_t
	{
		None,
		Reference,
		Pointer
	};

	enum class VariableType : uint8_t
	{
		Variable,
		Constant
	};
}