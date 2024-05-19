#pragma once

#include "../Node.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct NAMED_TYPE
	{
		Token* pNameToken;
	};

	struct STRUCT_TYPE
	{
		std::vector<std::pair<Token*, TYPE*>> Members;
	};

	struct ARRAY_TYPE
	{
		TYPE* pElementType;
		LITERAL* pSizeLiteral;
	};

	struct TYPE
	{
		TypeModifier Modifier;
		VariantNode<NAMED_TYPE, STRUCT_TYPE, ARRAY_TYPE> Type;
	};

	struct NAMED_TYPE_DEFINITION
	{
		Token* pNameToken;
		TYPE* pType;
	};
}