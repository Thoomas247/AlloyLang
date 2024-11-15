#pragma once

#include "NodeCommon.hpp"
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
		TYPE* pType;	// optional, if nullptr, try to infer type from the rhs of the expression
		bool isAny;		// set to true if a function parameter is of type Any
	};

	struct VARIABLE_DEFINITION
	{
		VARIABLE_DECLARATION* pDeclaration;
		EXPRESSION* pValue;
	};
}