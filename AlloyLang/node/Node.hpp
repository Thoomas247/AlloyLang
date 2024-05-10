#pragma once
#include "../log/Log.hpp"
#include "../tokenizer/Token.hpp"
#include "nodes/VariantNode.hpp"

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

	enum class VariableType : uint8_t
	{
		Variable,
		Constant
	};

	enum class NodeKind : uint8_t
	{
		None = -1
	};

	using PRIMARY = VariantNode<LITERAL, NAMED_VARIABLE, NAMED_VARIABLE_DEFINITION, FUNCTION_CALL, CONSTRUCTOR, 
		POINTER_INIT, POINTER_MOVE, INITIALIZER_LIST, ENCLOSED_EXPRESSION>;

	using STATEMENT = VariantNode<NAMED_VARIABLE_DEFINITION, FUNCTION_CALL, ASSIGNMENT, FOR_LOOP, WHILE_LOOP,
		IF_STATEMENT, STATEMENT_BLOCK, RETURN>;

	using POSTFIX = VariantNode<ARRAY_ACCESS, MEMBER_ACCESS>;

	using EXPRESSION = VariantNode<PRIMARY, POSTFIX, UNARY, BINARY, ASSIGNMENT>;
	
}