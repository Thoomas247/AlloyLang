#pragma once

#include "../Node.hpp"

namespace AlloyCompiler
{
	struct TYPE;
	struct EXPRESSION;

	struct NAMED_VARIABLE
	{
		const std::string_view Name;
	};

	struct NAMED_VARIABLE_DECLARATION
	{
		const VariableType VarType;
		const std::string_view Name;
		const TYPE* pType;
	};

	struct NAMED_VARIABLE_DEFINITION
	{
		const NAMED_VARIABLE_DECLARATION* pDeclaration;
		const EXPRESSION* pValue;
	};
}