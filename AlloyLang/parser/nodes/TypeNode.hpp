#pragma once

#include "../Node.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct MACRO_CALL;

	struct TYPE_NAME
	{
		Token* pNameToken;
		std::vector<TYPE*> GenericArguments;
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
		VariantNode<TYPE_NAME, STRUCT_TYPE, ARRAY_TYPE, MACRO_CALL> Type;
	};

	struct TYPE_DEFINITION
	{
		Token* pNameToken;
		TYPE* pType;

		std::vector<GENERIC_PARAMETER*> GenericParameters;
	};
}