#pragma once

#include "../Node.hpp"

namespace AlloyCompiler
{
	struct TYPE;

	struct NAMED_VARIABLE
	{
		std::string_view Name;
	};

	struct NAMED_VARIABLE_DECLARATION
	{
		VariableType VarType;
		std::string_view Name;
		TYPE* pType;
	};

	struct NAMED_VARIABLE_DEFINITION
	{
		NAMED_VARIABLE_DECLARATION* pDeclaration;
		EXPRESSION* pValue;
	};
}