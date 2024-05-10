#pragma once

#include "../Node.hpp"
#include "ExpressionNode.hpp"

namespace AlloyCompiler
{
	struct NAMED_VARIABLE_DECLARATION;
	struct STATEMENT_BLOCK;
	struct TYPE;

	struct FUNCTION_SIGNATURE 
	{
		std::string_view Name;
		std::vector<NAMED_VARIABLE_DECLARATION*> Parameters;
		RETURN_TYPE* pReturnType;	// optional
	};

	struct NAMED_FUNCTION_DEFINITION 
	{
		FUNCTION_SIGNATURE* pSignature;
		STATEMENT_BLOCK* pBody;
	};

	struct EXTERN_FUNCTION_DEFINITION
	{
		FUNCTION_SIGNATURE* pSignature;
	};

	struct RETURN_TYPE
	{
		VariableType VarType;
		TYPE* pType;
	};

	struct FUNCTION_CALL
	{
		std::string_view Function;
		std::vector<EXPRESSION*> Arguments;
	};

}