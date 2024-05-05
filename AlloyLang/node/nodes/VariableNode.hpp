#pragma once

#include "../Node.hpp"

namespace AlloyCompiler
{
	struct TYPE;

	struct NAMED_VARIABLE : DEFINITION, ASSIGNABLE, PRIMARY
	{
		const std::string_view Name;
	};

	struct NAMED_VARIABLE_DECLARATION : NODE
	{
		const VariableType VarType;
		const std::string_view Name;
		const TYPE* pType;
	};

	struct NAMED_VARIABLE_DEFINITION : STATEMENT, PRIMARY
	{
		const NAMED_VARIABLE_DECLARATION* pDeclaration;
		const EXPRESSION* pValue;
	};
}