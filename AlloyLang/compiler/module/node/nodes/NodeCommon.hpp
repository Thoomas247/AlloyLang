#pragma once
#include <vector>

#include "../../token/Token.hpp"

namespace AlloyCompiler
{
	enum class LiteralType : uint8_t
	{
		Integer,
		Float,
		Boolean,
		String,
		Character
	};

	enum class TypeModifier : uint8_t
	{
		None,
		Reference,
		Pointer
	};

	enum class ReturnType : uint8_t
	{
		Copy,
		Variable,
		Constant
	};

	enum class VariableType
	{
		Variable,
		Constant
	};

	enum class GenericParameterType : uint8_t
	{
		Fn, Type
	};

}