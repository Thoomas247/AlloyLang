#pragma once

#include "../Node.hpp"
#include "ExpressionNode.hpp"

namespace AlloyCompiler
{
	struct NAMED_VARIABLE_DECLARATION;
	struct STATEMENT_BLOCK;
	struct TYPE;

	struct NAMED_FUNCTION : NODE
	{
		const std::string_view Name;
	};

	struct FUNCTION_SIGNATURE : NODE
	{
		const std::string_view Name;
		const std::vector<NAMED_VARIABLE_DECLARATION*> Parameters;
		const RETURN_TYPE* pReturnType;	// optional
	};

	struct NAMED_FUNCTION_DEFINITION : DEFINITION
	{
		const FUNCTION_SIGNATURE* pSignature;
		const STATEMENT_BLOCK* pBlock;
	};

	struct EXTERN_FUNCTION_DEFINITION : DEFINITION
	{
		const FUNCTION_SIGNATURE* pSignature;
	};

	struct RETURN_TYPE : NODE
	{
		const VariableType VarType;
		const TYPE* pType;
	};

	struct FUNCTION_CALL : PRIMARY, STATEMENT
	{
		const NAMED_FUNCTION* pFunction;
		const std::vector<EXPRESSION*> Arguments;
	};

}