#pragma once
#include "../log/Log.hpp"
#include "../tokenizer/Token.hpp"
#include "nodes/VariantNode.hpp"

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

	enum class NodeKind : uint8_t
	{
		None = -1
	};

	using EXPRESSION = VariantNode;
	using STATEMENT = VariantNode;
}