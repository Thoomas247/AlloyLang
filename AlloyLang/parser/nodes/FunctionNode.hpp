#pragma once

#include "../Node.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct NAMED_VARIABLE_DECLARATION;
	struct STATEMENT_BLOCK;
	struct RETURN_TYPE;

	struct FUNCTION_SIGNATURE 
	{
		std::string_view Name;
		bool IsVarArg;
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
		ReturnType RetType;
		TYPE* pType;
	};

	struct FUNCTION_CALL
	{
		std::string_view Function;
		std::vector<EXPRESSION*> Arguments;
	};

}