#pragma once
#include "../log/Log.hpp"
#include "../tokenizer/Token.hpp"
#include "VariantNode.hpp"

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

	enum class VariableType : uint8_t
	{
		Variable,
		Constant
	};
	
}