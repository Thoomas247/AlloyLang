#pragma once

#include "../Node.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct NAMED_VARIABLE
	{
		Token* pNameToken;
	};

	struct NAMED_VARIABLE_DECLARATION
	{
		VariableType VarType;
		Token* pNameToken;
		TYPE* pType;
	};

	struct NAMED_VARIABLE_DEFINITION
	{
		NAMED_VARIABLE_DECLARATION* pDeclaration;
		EXPRESSION* pValue;
	};
}