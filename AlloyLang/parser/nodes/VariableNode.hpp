#pragma once

#include "../Node.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct VARIABLE
	{
		Token* pNameToken;
	};

	struct VARIABLE_DECLARATION
	{
		VariableType VarType;
		Token* pNameToken;
		TYPE* pType;
	};

	struct VARIABLE_DEFINITION
	{
		VARIABLE_DECLARATION* pDeclaration;
		EXPRESSION* pValue;
	};
}