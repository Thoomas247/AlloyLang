#pragma once

#include "../Node.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct VARIABLE_DECLARATION;
	struct STATEMENT_BLOCK;
	struct RETURN_TYPE;

	struct FUNCTION_SIGNATURE 
	{
		Token* pNameToken;
		bool IsVarArg;
		std::vector<VARIABLE_DECLARATION*> Parameters;
		RETURN_TYPE* pReturnType;	// optional
	};

	struct NAMED_FUNCTION_DEFINITION 
	{
		FUNCTION_SIGNATURE* pSignature;
		STATEMENT_BLOCK* pBody;
	};

	struct EXTERN_DEFINITION
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
		Token* pFunctionNameToken;
		std::vector<EXPRESSION*> Arguments;
	};

}